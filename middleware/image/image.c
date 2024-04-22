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

ringbuffer_typedef(rgb_frame_t, rgbImageBuffer_t);

typedef struct
{
    osal_mutex_t write_mtx;
    osal_mutex_t read_mtx;
}buffer_mtx_handle_t;

static rgbImageBuffer_t imageBuffer;
static buffer_mtx_handle_t buf_mtx_handle;
static UINT08 frameIdx;

static BOOL_T imagebuffer_initialized = DEF_FALSE;

static void yuv2rgb(INT32 y, INT32 u, INT32 v, UINT08 *r, UINT08 *g, UINT08 *b);
static sys_result_e rgb888_convert(UINT32 srcFmt, const UINT08 *src_frame, UINT08 *dest_frame);
static INT32 file_write_blocking(INT32 fd, const UINT08 *buf, size_t size);

sys_result_e imagebuffer_init(void)
{
    if (imagebuffer_initialized)
        return SYS_SUCCESS;

    osal_mutex_init(&buf_mtx_handle.write_mtx, OSAL_MTX_PRIO_INHERIT);
    osal_mutex_init(&buf_mtx_handle.read_mtx,  OSAL_MTX_PRIO_INHERIT);

    ringbuffer_init(imageBuffer, rgb_frame_t, 8);

    return SYS_SUCCESS;
}
sys_result_e imagebuffer_startread(rgb_frame_t *frame)
{
    sys_result_e ret;
    osal_mutex_lock(&buf_mtx_handle.read_mtx);

    if (!ringbuffer_isEmpty(&imageBuffer))
    {
        ringbuffer_read_zc(&imageBuffer, frame);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_IGNORE;
    }

    osal_mutex_unlock(&buf_mtx_handle.read_mtx);

    return ret;
}
sys_result_e imagebuffer_endread(void)
{
    sys_result_e ret;
    osal_mutex_lock(&buf_mtx_handle.read_mtx);

    if (!ringbuffer_isEmpty(&imageBuffer))
    {
        ringbuffer_inc_readptr(&imageBuffer);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_IGNORE;
    }

    osal_mutex_unlock(&buf_mtx_handle.read_mtx);

    return ret;
}

sys_result_e imagebuffer_write(rgb_frame_t *frame)
{
    sys_result_e ret;
    osal_mutex_lock(&buf_mtx_handle.write_mtx);
    if (!ringbuffer_isFull(&imageBuffer))
    {
        ringbuffer_write(&imageBuffer, *frame);
        ret = SYS_SUCCESS;
    }
    else
    {
        ret = SYS_FAILURE;
    }

    osal_mutex_unlock(&buf_mtx_handle.write_mtx);

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
            rgb888_convert(srcFmt, src_frame, dest_frame);
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

    *r = r1;
    *g = g1;
    *b = b1;
}

sys_result_e image_save(const UINT08 *buf, const UINT32 size)
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

    ret = file_write_blocking(file, buf, size);
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

static INT32 file_write_blocking(INT32 fd, const UINT08 *buf, size_t size)
{
    static UINT08 processBuf[(PIXEL_WIDTH * PIXEL_HEIGHT)];

    /* Bytes written to file */
    ssize_t wBytes = 0;

    size_t sizeBuf = size;

    UINT32 i = 0;
    char header[] = "P6\nRESERVED\n320 240\n255\n";

    for (UINT32 idx = 0; idx < ((PIXEL_WIDTH * PIXEL_HEIGHT)); idx++)
    {
        processBuf[idx] = buf[i];

        i = i + 2u;
    }

    // subtract 1 because sizeof for string includes null terminator
    write(fd, header, sizeof(header) - 1);

    do
    {
        wBytes += write(fd, &processBuf[wBytes], sizeBuf);

        if (wBytes > 0)
            sizeBuf -= (size_t)wBytes;

    } while (wBytes < (size >> 1));

    return wBytes;
}