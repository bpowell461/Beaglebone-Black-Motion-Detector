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

static UINT08 *frameBuffer;
static INT32 framebuffer_fd;
static UINT32 frameBufferSize = 0;
static UINT16 frameIdx = 0;
static osal_mutex_t mtx;

static sys_result_e framebuffer_ioctl(INT32 fd, INT32 request, void *arg);

sys_result_e framebuffer_init(INT32 *fd, UINT08 bufCount)
{
    osal_mutex_init(&mtx, OSAL_MTX_PRIO_INHERIT);

    /* V4L2 Buffer Vars */
    struct v4l2_requestbuffers req = { 0 };
    struct v4l2_buffer buf = { 0 };

    framebuffer_fd = *fd;

    /* Setting up V4L2 Buffers */
    req.count = bufCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (SYS_FAILURE == framebuffer_ioctl(framebuffer_fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        exit(1);
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (SYS_FAILURE == framebuffer_ioctl(framebuffer_fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Could not query buffer");
        exit(1);
    }

    frameBuffer = (UINT08 *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, buf.m.offset);

    frameBufferSize = buf.length;

    return SYS_SUCCESS;
}

sys_result_e framebuffer_writeframe(INT32 fd)
{
    osal_mutex_lock(&mtx);

    struct v4l2_buffer bufd = { 0 };
    bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufd.memory = V4L2_MEMORY_MMAP;
    bufd.index = 0;
    if (SYS_SUCCESS != framebuffer_ioctl(fd, VIDIOC_QBUF, &bufd))
    {
        osal_mutex_unlock(&mtx);
        return SYS_FAILURE;
    }

    osal_mutex_unlock(&mtx);
    return SYS_SUCCESS;
}
sys_result_e framebuffer_getframe(INT32 fd)
{
    struct v4l2_buffer buf = { 0 };
    fd_set fds;
    char out_name[256];

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    int r = select(fd + 1, &fds, NULL, NULL, NULL);
    if (-1 == r) {
        return SYS_IGNORE;
    }

    osal_mutex_lock(&mtx);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    framebuffer_ioctl(fd, VIDIOC_DQBUF, &buf);

    sprintf(out_name, "frame%03d.yuy", frameIdx);
    INT32 file = open(out_name, O_WRONLY);
    write(file, frameBuffer, frameBufferSize);
    close(file);
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
    munmap(frameBuffer, frameBufferSize);
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
