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
static struct timeval start_time;

#define NUM_FRAME_BUFS 32

struct buffer {
    uint8_t *start;
    size_t size;
};

static struct buffer buffers[NUM_FRAME_BUFS];

static int framebuffer_fd;
static uint32_t frame_cnt = 0;
static uint32_t save_cnt = 0;

static sys_result_e framebuffer_ioctl(int fd, uint32_t request, void *arg);
static uint32_t       framebuffer_requestbuffers(uint8_t count);
static uint32_t       framebuffer_mapbuffers(uint8_t idx, uint8_t **bufferPtr);
static sys_result_e       framebuffer_queueframe(struct v4l2_buffer *buf);
static sys_result_e framebuffer_dequeueframe(struct v4l2_buffer *buf);

sys_result_e framebuffer_init(int *fd)
{
    uint32_t bufCount = 0;

    framebuffer_fd = *fd;

    bufCount = framebuffer_requestbuffers(NUM_FRAME_BUFS);

    for (uint8_t i = 0; i < bufCount; i++) 
    {
        buffers[i].size = framebuffer_mapbuffers(i, &buffers[i].start);
    }

    ringbuffer_init(incomingBuffer, frame_t, 128);

    framebuffer_initframebuffers(framebuffer_fd);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_getframe(int fd, frame_t *frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_IGNORE;
    }

    ringbuffer_read(&incomingBuffer, *frame);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_getframe_ptr(int fd, frame_t **frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_IGNORE;
    }

    ringbuffer_read_zc(&incomingBuffer, *frame);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_freeframe(int fd, frame_t *frame)
{
    if (ringbuffer_isEmpty(&incomingBuffer))
    {
        return SYS_FAILURE;
    }

    ringbuffer_inc_readptr(&incomingBuffer);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_initframebuffers(int fd)
{
    struct v4l2_buffer bufd = { 0 };

    for (uint8_t i = 0; i < NUM_FRAME_BUFS; i++)
    {
        CLEAR(bufd);
        bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufd.memory = V4L2_MEMORY_MMAP;
        bufd.index = i;
        (void)framebuffer_queueframe(&bufd);
    }

    return SYS_SUCCESS;
}

sys_result_e framebuffer_writeframe(int fd, uint8_t saveFrame)
{
    fd_set fds;
    struct v4l2_buffer buf = { 0 };

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    uint8_t writeCondition = saveFrame && (frame_cnt % OVERSAMPLE_FRAME == 0);

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int r = select(fd + 1, &fds, NULL, NULL, 0);
    if (-1 == r) {
        return SYS_IGNORE;
    }
    
    if (SYS_SUCCESS != framebuffer_dequeueframe(&buf))
    {
        return SYS_FAILURE;
    }

    if (writeCondition)
    {
        if (!ringbuffer_isFull(&incomingBuffer))
        {
            if (!save_cnt)
            {
                start_time = buf.timestamp;
            }

            ringbuffer_memcpy(incomingBuffer.data[incomingBuffer.writePtr].bytes, buffers[buf.index].start, buf.bytesused);

            timersub(&buf.timestamp, &start_time, &incomingBuffer.data[incomingBuffer.writePtr].timestamp);

            save_cnt++;

            ringbuffer_inc_writeptr(&incomingBuffer);
        }
        else
        {
            SYS_TRACE("ERR: Framebuffer full");
        }
    }

    if (SYS_SUCCESS != framebuffer_queueframe(&buf))
    {
        return SYS_FAILURE;
    }

    if(saveFrame)
        frame_cnt++;

    if (writeCondition)
    {
        return SYS_SUCCESS;
    }
    else
    {
        return SYS_IGNORE;
    }
}

sys_result_e framebuffer_deinit(void)
{
    close(framebuffer_fd);
    for (uint32_t i = 0; i < NUM_FRAME_BUFS; ++i)
        munmap(buffers[i].start, buffers[i].size);

    return SYS_SUCCESS;
}

static sys_result_e framebuffer_ioctl(int fd, uint32_t request, void *arg)
{
    int ret = ioctl(fd, request, arg);
    if (EAGAIN == ret || EPIPE == ret)
    {
        return SYS_IGNORE;
    }
    else if (0 == ret)
    {
        return SYS_SUCCESS;
    }
    else
    {
        return SYS_FAILURE;
    }
}

static uint32_t framebuffer_requestbuffers(uint8_t count)
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

static uint32_t framebuffer_mapbuffers(uint8_t idx, uint8_t **bufferPtr)
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

    *bufferPtr = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, (off_t)buf.m.offset);

    return buf.length;
}

static sys_result_e framebuffer_queueframe(struct v4l2_buffer *bufd)
{
    if (SYS_SUCCESS != framebuffer_ioctl(framebuffer_fd, VIDIOC_QBUF, bufd))
    {
        SYS_TRACE("ERR: FRAMEBUFFER QBUF");
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

static sys_result_e framebuffer_dequeueframe(struct v4l2_buffer *buf)
{
    sys_result_e ret = framebuffer_ioctl(framebuffer_fd, VIDIOC_DQBUF, buf);
    if (SYS_FAILURE == ret)
    {
        SYS_TRACE("ERR: FRAMEBUFFER DQBUF");
    }

    return ret;
}
