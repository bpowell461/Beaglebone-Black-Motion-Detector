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
#include "ringbuffer.h"
#include "utils.h"
#include "syslog.h"
#include "osal.h"
#include "camera_cfg.h"

//#define USE_ALT_FILE_SAVE

typedef struct
{
    UINT08 bytes[PIXEL_BYTES];
}frame_t;

ringbuffer_typedef(frame_t, frameBuffer_t);

static frameBuffer_t frameBuffer;

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

static UINT32       framebuffer_allocatebuffers(UINT08 count);
static UINT32       framebuffer_queueframe(UINT08 idx);
static sys_result_e framebuffer_save(frame_t *buf);
static INT32 file_write_blocking(INT32 fd, const UINT08 *buf, size_t size);

sys_result_e framebuffer_init(INT32 *fd)
{
    osal_mutex_init(&mtx, OSAL_MTX_PRIO_INHERIT);

    framebuffer_fd = *fd;

    (void)framebuffer_allocatebuffers(NUM_FRAME_BUFS);

    return SYS_SUCCESS;
}

sys_result_e framebuffer_writeframe(INT32 fd)
{
    UINT32 bytes = 0;
    osal_mutex_lock(&mtx);

    bytes = framebuffer_queueframe(writePtr);

    osal_mutex_unlock(&mtx);

    
    return (bytes ? SYS_SUCCESS : SYS_FAILURE);
}

sys_result_e framebuffer_getframe(INT32 fd)
{
    frame_t buf;

    if (ringbuffer_isEmpty(&frameBuffer))
    {
        return SYS_IGNORE;
    }

    osal_mutex_lock(&mtx);

    ringbuffer_read(&frameBuffer, buf);

    framebuffer_save(&buf);
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

static UINT32 framebuffer_allocatebuffers(UINT08 count)
{
    ringbuffer_init(frameBuffer, frame_t, NUM_FRAME_BUFS);

    return NUM_FRAME_BUFS;
}

static UINT32 framebuffer_queueframe(UINT08 idx)
{
    frame_t buf;
    ssize_t bytesused = read(framebuffer_fd, buf.bytes, PIXEL_BYTES);

    if (0 > bytesused)
    {
        return 0;
    }

    if (bytesused && !ringbuffer_isFull(&frameBuffer))
    {
        ringbuffer_write(&frameBuffer, buf);
    }

    return (UINT32)bytesused;
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
    if (0 > fwrite(buffers[index].start, buffers[index].size, 1, fout))
    {
        SYS_TRACE("Failed to write to file!");
    }
    fclose(fout);

    return SYS_SUCCESS;
}

#else

static sys_result_e framebuffer_save(frame_t *buf)
{
    char out_name[256];
    INT32 ret;

    sprintf(out_name, IMAGE_FILE("frame%03d"), frameIdx);
    INT32 file = open(out_name, O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK, 0666);
    if (0 > file)
    {
        SYS_TRACE("ERR: FILE OPEN");
        return SYS_FAILURE;
    }

    /*snprintf(image_header, 16, "P6\n%d %d 255\n", PIXEL_WIDTH, PIXEL_HEIGHT);
    write(file, image_header, 16);*/
    ret = file_write_blocking(file, buf->bytes, PIXEL_BYTES);
    if (0 > ret)
    {
        SYS_TRACE("ERR: WRITE");
        return SYS_FAILURE;
    }

    close(file);

    return SYS_SUCCESS;
}


static INT32 file_write_blocking(INT32 fd, const UINT08 *buf, size_t size)
{
    ssize_t wBytes = 0;
    size_t sizeBuf = size;
    do
    {
        wBytes += write(fd, &buf[wBytes], sizeBuf);

        if(wBytes > 0)
            sizeBuf -= (size_t)wBytes;

    } while (wBytes < size);

    return wBytes;
}
#endif
