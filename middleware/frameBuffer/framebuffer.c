#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include "framebuffer.h"
#include "utils.h"
#include "syslog.h"
#include "osal.h"
#include "camera_cfg.h"

ringbuffer_typedef(frame_t, rawImageBuffer_t);

static rawImageBuffer_t incomingBuffer;

#define NUM_FRAME_BUFS 8

struct buffer {
    UINT08 *start;
    size_t size;
};

static struct buffer buffers[NUM_FRAME_BUFS];

static INT32 framebuffer_fd;

static struct timeval select_timeout;

static osal_mutex_t mtx;

static sys_result_e framebuffer_ioctl(INT32 fd, UINT32 request, void *arg);
static UINT32       framebuffer_requestbuffers(UINT08 count);
static UINT32       framebuffer_mapbuffers(UINT08 idx, UINT08 **bufferPtr);
static UINT32       framebuffer_queueframe(struct v4l2_buffer *buf);
static sys_result_e framebuffer_dequeueframe(struct v4l2_buffer *buf);

sys_result_e framebuffer_init(INT32 *fd)
{
    UINT32 bufCount = 0;
    osal_mutex_init(&mtx, OSAL_MTX_PRIO_INHERIT);

    framebuffer_fd = *fd;

    bufCount = framebuffer_requestbuffers(NUM_FRAME_BUFS);

    for (UINT08 i = 0; i < bufCount; i++) 
    {
        buffers[i].size = framebuffer_mapbuffers(i, &buffers[i].start);
    }

    ringbuffer_init(incomingBuffer, frame_t, NUM_FRAME_BUFS);

    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = (900 * USEC_PER_MSEC);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_getframe(INT32 fd, frame_t *frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_IGNORE;
    }

    osal_mutex_lock(&mtx);

    ringbuffer_read(&incomingBuffer, *frame);

    osal_mutex_unlock(&mtx);


    return SYS_SUCCESS;
}

sys_result_e framebuffer_getframe_ptr(INT32 fd, frame_t **frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_IGNORE;
    }

    osal_mutex_lock(&mtx);

    ringbuffer_read_zc(&incomingBuffer, *frame);

    osal_mutex_unlock(&mtx);

    
    return SYS_SUCCESS;
}

sys_result_e framebuffer_freeframe(INT32 fd, frame_t *frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_FAILURE;
    }

    osal_mutex_lock(&mtx);

    ringbuffer_inc_readptr(&incomingBuffer);

    osal_mutex_unlock(&mtx);


    return SYS_SUCCESS;
}

sys_result_e framebuffer_initframebuffers(INT32 fd)
{
    struct v4l2_buffer bufd = { 0 };

    for (UINT08 i = 0; i < NUM_FRAME_BUFS; i++)
    {
        CLEAR(bufd);
        bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufd.memory = V4L2_MEMORY_MMAP;
        bufd.index = i;
        (void)framebuffer_queueframe(&bufd);
    }

    return SYS_SUCCESS;
}

sys_result_e framebuffer_writeframe(INT32 fd)
{
    fd_set fds;
    sys_result_e ret;
    struct v4l2_buffer buf = { 0 };

    /* Apparently select MAY modify the timeval struct */
    struct timeval temp = select_timeout;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int r = select(fd + 1, &fds, NULL, NULL, &temp);
    if (-1 == r) {
        return SYS_IGNORE;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    osal_mutex_lock(&mtx);

    ret = framebuffer_dequeueframe(&buf);

    if (SYS_SUCCESS == ret)
    {
        if (!ringbuffer_isFull(&incomingBuffer))
        {
            memcpy(incomingBuffer.data[incomingBuffer.writePtr].bytes, buffers[buf.index].start, buffers[buf.index].size);
            ringbuffer_inc_writeptr(&incomingBuffer);
        }
    }

    framebuffer_queueframe(&buf);
    
    osal_mutex_unlock(&mtx);

    return ret;
}

sys_result_e framebuffer_deinit(void)
{
    close(framebuffer_fd);
    for (UINT32 i = 0; i < NUM_FRAME_BUFS; ++i)
        munmap(buffers[i].start, buffers[i].size);

    return SYS_SUCCESS;
}

static sys_result_e framebuffer_ioctl(INT32 fd, UINT32 request, void *arg)
{
    if (-1 == ioctl(fd, request, arg))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

static UINT32 framebuffer_requestbuffers(UINT08 count)
{
    /* V4L2 Buffer Vars */
    struct v4l2_requestbuffers req = { 0 };

    /* Requesting V4L2 Buffers */
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (SYS_FAILURE == framebuffer_ioctl(framebuffer_fd, VIDIOC_REQBUFS, &req))
    {
        SYS_TRACE("ERR: Requesting Buffer");
        exit(1);
    }

    if (count < req.count)
    {
        SYS_TRACE("ERR: Increase V4L buffers to %u\n", req.count);
        exit(1);
    }

    return req.count;
}

static UINT32 framebuffer_mapbuffers(UINT08 idx, UINT08 **bufferPtr)
{
    struct v4l2_buffer buf = { 0 };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = idx;

    if (SYS_FAILURE == framebuffer_ioctl(framebuffer_fd, VIDIOC_QUERYBUF, &buf))
    {
        SYS_TRACE("ERR: Querying Buffer");
        exit(1);
    }

    *bufferPtr = (UINT08 *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, (off_t)buf.m.offset);

    return buf.length;
}

static UINT32 framebuffer_queueframe(struct v4l2_buffer *bufd)
{
    if (SYS_SUCCESS != framebuffer_ioctl(framebuffer_fd, VIDIOC_QBUF, bufd))
    {
        SYS_TRACE("ERR: FRAMEBUFFER QBUF");
        return SYS_FAILURE;
    }

    return bufd->bytesused;
}

static sys_result_e framebuffer_dequeueframe(struct v4l2_buffer *buf)
{
    if (SYS_SUCCESS != framebuffer_ioctl(framebuffer_fd, VIDIOC_DQBUF, buf))
    {
        SYS_TRACE("ERR: FRAMEBUFFER DQBUF");
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}
