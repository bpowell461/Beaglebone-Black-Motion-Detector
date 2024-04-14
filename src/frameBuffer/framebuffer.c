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

//#define USE_ALT_FILE_SAVE

#define NUM_FRAME_BUFS 16

struct buffer {
    UINT08 *start;
    size_t size;
};

static struct buffer buffers[NUM_FRAME_BUFS];
//static struct buffer *buffers;

static INT32 framebuffer_fd;
static UINT16 frameIdx = 0;

static UINT08 writePtr = 0;

static osal_mutex_t mtx;

static sys_result_e framebuffer_ioctl(INT32 fd, INT32 request, void *arg);
static UINT32       framebuffer_requestbuffers(UINT08 count);
static UINT32       framebuffer_mapbuffers(UINT08 idx, UINT08 **bufferPtr);
static UINT32       framebuffer_queueframe(UINT08 idx);
static UINT32       framebuffer_dequeueframe(void);
static sys_result_e framebuffer_save(UINT32 index);

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

    return SYS_SUCCESS;
}

sys_result_e framebuffer_writeframe(INT32 fd)
{
    UINT32 bytes = 0;
    osal_mutex_lock(&mtx);

    bytes = framebuffer_queueframe(writePtr);

    if(bytes)
        writePtr = (writePtr + 1) % NUM_FRAME_BUFS;

    osal_mutex_unlock(&mtx);

    
    return (bytes ? SYS_SUCCESS : SYS_FAILURE);
}

sys_result_e framebuffer_getframe(INT32 fd)
{
    fd_set fds;
    UINT32 readIdx;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int r = select(fd + 1, &fds, NULL, NULL, NULL);
    if (-1 == r) {
        return SYS_IGNORE;
    }

    osal_mutex_lock(&mtx);

    readIdx = framebuffer_dequeueframe();
    framebuffer_save(readIdx);
    frameIdx++;

    osal_mutex_unlock(&mtx);

    return SYS_SUCCESS;
}

UINT16 framebuffer_getframeidx(void)
{
    return frameIdx;
}

sys_result_e framebuffer_deinit(void)
{
    close(framebuffer_fd);
    for (UINT32 i = 0; i < NUM_FRAME_BUFS; ++i)
        munmap(buffers[i].start, buffers[i].size);

    return SYS_SUCCESS;
}

static sys_result_e framebuffer_ioctl(INT32 fd, INT32 request, void *arg)
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

    *bufferPtr = (UINT08 *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, buf.m.offset);

    return buf.length;
}

static UINT32 framebuffer_queueframe(UINT08 idx)
{
    struct v4l2_buffer bufd = { 0 };
    bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufd.memory = V4L2_MEMORY_MMAP;
    bufd.index = idx;
    if (SYS_SUCCESS != framebuffer_ioctl(framebuffer_fd, VIDIOC_QBUF, &bufd))
    {
        osal_mutex_unlock(&mtx);
        return SYS_FAILURE;
    }

    return bufd.bytesused;
}

static UINT32 framebuffer_dequeueframe(void)
{
    struct v4l2_buffer buf = { 0 };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (SYS_SUCCESS != framebuffer_ioctl(framebuffer_fd, VIDIOC_DQBUF, &buf))
    {
        SYS_TRACE("ERR: FRAMEBUFFER DQBUF");
        return SYS_FAILURE;
    }

    return buf.index;
}


#if defined(USE_ALT_FILE_SAVE)

static sys_result_e framebuffer_save(UINT32 index)
{
    FILE *fout;
    char out_name[256];
    sprintf(out_name, "frame%03d.ppm", frameIdx);
    fout = fopen(out_name, "w");
    if (!fout)
    {
        SYS_TRACE("Cannot open image");
        return (SYS_FAILURE);
    }
    fprintf(fout, "P6\n%d %d 255\n", PIXEL_WIDTH, PIXEL_HEIGHT);
    if (0 > fwrite(buffers[buf.index].start, buf.bytesused, 1, fout))
    {
        SYS_TRACE("Failed to write to file!");
    }
    fclose(fout);

    return SYS_SUCCESS;
}

#else

static sys_result_e framebuffer_save(UINT32 index)
{
    char out_name[256];
    INT32 ret;
    sprintf(out_name, IMAGE_FILE("frame%03d"), frameIdx);
    INT32 file = open(out_name, O_RDWR | O_CREAT, 0666);
    if (0 > file)
    {
        SYS_TRACE("ERR: FILE OPEN");
        return SYS_FAILURE;
    }

    ret = write(file, buffers[index].start, buffers[index].size);
    if (0 > ret)
    {
        SYS_TRACE("ERR: WRITE");
        return SYS_FAILURE;
    }

    close(file);

    return SYS_SUCCESS;
}
#endif
