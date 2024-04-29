#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "image.h"
#include "ringbuffer.h"
#include "osal.h"
#include "utils.h"
#include "syslog.h"

SYSLOG_INITMEASURE();

ringbuffer_typedef(rgb_frame_t, rgbImageBuffer_t);

typedef struct
{
    osal_mutex_t write_mtx;
    osal_mutex_t read_mtx;
}buffer_mtx_handle_t;

static rgbImageBuffer_t imageBuffer;
static buffer_mtx_handle_t buf_mtx_handle;
static UINT08 frameIdx = 0;

static BOOL_T imagebuffer_initialized = DEF_FALSE;

static struct timespec start_time;

static void yuv2rgb(INT32 y, INT32 u, INT32 v, UINT08 *r, UINT08 *g, UINT08 *b);
static sys_result_e rgb888_convert(UINT32 srcFmt, const UINT08 *src_frame, UINT08 *dest_frame);
static INT32 file_write_blocking(INT32 fd, const rgb_frame_t *buf, size_t size);

sys_result_e imagebuffer_init(void)
{
    if (imagebuffer_initialized)
        return SYS_SUCCESS;

    ringbuffer_init(imageBuffer, rgb_frame_t, 32);

    return SYS_SUCCESS;
}
sys_result_e imagebuffer_startread(rgb_frame_t **frame)
{
    sys_result_e ret;

    if (!ringbuffer_isEmpty(&imageBuffer))
    {
        ringbuffer_read_zc(&imageBuffer, *frame);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_IGNORE;
    }

    return ret;
}
sys_result_e imagebuffer_endread(void)
{
    sys_result_e ret;

    if (!ringbuffer_isEmpty(&imageBuffer))
    {
        ringbuffer_inc_readptr(&imageBuffer);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_IGNORE;
    }

    return ret;
}

sys_result_e imagebuffer_write(rgb_frame_t *frame)
{
    sys_result_e ret;
    if (!ringbuffer_isFull(&imageBuffer))
    {
        ringbuffer_write(&imageBuffer, *frame);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_FAILURE;
    }

    return ret;
}

sys_result_e image_convert(UINT32 srcFmt, UINT32 destFmt, const UINT08 *src_frame, UINT08 *dest_frame)
{
    if (srcFmt == destFmt)
    {
        return SYS_IGNORE;
    }

    switch (destFmt)
    {
        case V4L2_PIX_FMT_RGB888:
        {
            SYSLOG_MEASURE(rgb888_convert(srcFmt, src_frame, dest_frame), "rgb888_convert");
        }
        default:
        {
            /* Catch all for formats not supported yet */
            return SYS_FAILURE;
        }
    }

}

static sys_result_e rgb888_convert(UINT32 srcFmt, const UINT08 *src_frame, UINT08 *dest_frame)
{
    UINT32 i = 0;
    UINT32 j = 0;
    INT32 y_temp, y2_temp, u_temp, v_temp;
    switch (srcFmt)
    {
        case V4L2_PIX_FMT_YUYV:
        {
            for (i = 0, j = 0; i < (FRAME_SIZE); i = i + 4, j = j + 6)
            {
                /* Minor optimization with loop unrolling */
                y_temp  = (INT32)src_frame[i + 0];
                u_temp  = (INT32)src_frame[i + 1];
                y2_temp = (INT32)src_frame[i + 2];
                v_temp  = (INT32)src_frame[i + 3];
                yuv2rgb(y_temp, u_temp, v_temp, &dest_frame[j], &dest_frame[j + 1], &dest_frame[j + 2]);
                yuv2rgb(y2_temp, u_temp, v_temp, &dest_frame[j + 3], &dest_frame[j + 4], &dest_frame[j + 5]);
            }
        }
        default:
        {
            /* Catch all for formats not supported yet */
            return SYS_FAILURE;
        }
    }

    return SYS_SUCCESS;
}

static void yuv2rgb(INT32 y, INT32 u, INT32 v, UINT08 *r, UINT08 *g, UINT08 *b)
{
    /* For now this is a manual conversion of YUV to RGB888.
       
       This could be reconstructed to use a color lookup table (CLUT)
       which would greatly decrease the time spent calculating RGB values.
    */

    INT32 r1, g1, b1;

    // replaces floating point coefficients
    INT32 c = y - 16, d = u - 128, e = v - 128;

    // Conversion that avoids floating point
    r1 = (298 * c + 409 * e + 128) >> 8;
    g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
    b1 = (298 * c + 516 * d + 128) >> 8;

    // Computed values may need clipping.
    CLAMP_UPPER(r1, 255);
    CLAMP_LOWER(r1, 0);

    CLAMP_UPPER(g1, 255);
    CLAMP_LOWER(g1, 0);

    CLAMP_UPPER(b1, 255);
    CLAMP_LOWER(b1, 0);

    *r = (UINT08) r1 & 0xFFu;
    *g = (UINT08) g1 & 0xFFu;
    *b = (UINT08) b1 & 0xFFu;
}

sys_result_e image_save(const rgb_frame_t *buf, const UINT32 size)
{
    char out_name[256];
    INT32 ret;

    sprintf(out_name, IMAGE_FILE("/media/card/pics/frame%03d"), frameIdx);
    INT32 file = open(out_name, O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK, 0666);
    if (0 > file)
    {
        SYS_TRACE("ERR: FILE OPEN");
        return SYS_FAILURE;
    }

    SYSLOG_MEASURE(ret = file_write_blocking(file, buf, size), "file_write_blocking");
    if (0 > ret)
    {
        SYS_TRACE("ERR: WRITE");
        return SYS_FAILURE;
    }

    frameIdx++;

    close(file);

    return SYS_SUCCESS;
}

UINT08 image_getsavedframes(void)
{
    return frameIdx;
}

static INT32 file_write_blocking(INT32 fd, const rgb_frame_t *buf, size_t size)
{
    /* Bytes written to file */
    ssize_t wBytes = 0;

    size_t sizeBuf = size;

    char header[256];

    if (!frameIdx)
    {
        start_time = buf->timestamp;
    }

    sprintf(header, IMAGE_HEADER, syslog_getsysname(), ((buf->timestamp.tv_nsec - start_time.tv_nsec) / (long)(USEC_PER_MSEC * NSEC_PER_USEC)));

    write(fd, header, strlen(header));

    do
    {
        wBytes += write(fd, &buf->bytes[wBytes], sizeBuf);

        if (wBytes > 0)
            sizeBuf -= (size_t)wBytes;

    } while (wBytes < (size >> 1));

    return wBytes;
}