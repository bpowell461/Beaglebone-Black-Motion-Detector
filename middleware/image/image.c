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
#include <sys/socket.h>
#include <arpa/inet.h>

#define STREAM_TO_SERVER

ringbuffer_typedef(rgb_frame_t, rgbImageBuffer_t);

static rgbImageBuffer_t imageBuffer;
static uint32_t frameIdx = 0;
static int sockfd = -1;
static struct sockaddr_in pc_addr;

static uint8_t imagebuffer_initialized = false;

static void yuv2rgb(int y, int u, int v, uint8_t *r, uint8_t *g, uint8_t *b);
static sys_result_e rgb888_convert(uint32_t srcFmt, const uint8_t *src_frame, uint8_t *dest_frame);
static int file_write_blocking(int fd, const void *buf, size_t size);

sys_result_e imagebuffer_init(void)
{
    if (imagebuffer_initialized)
        return SYS_SUCCESS;

    ringbuffer_init(imageBuffer, rgb_frame_t, 256);

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

sys_result_e image_convert(uint32_t srcFmt, uint32_t destFmt, const uint8_t *src_frame, uint8_t *dest_frame)
{
    if (srcFmt == destFmt)
    {
        return SYS_IGNORE;
    }

    switch (destFmt)
    {
        case V4L2_PIX_FMT_RGB888:
        {
            return rgb888_convert(srcFmt, src_frame, dest_frame);
        }
        case V4L2_PIX_FMT_MJPEG:
        {
            /* Not supported yet */
            return SYS_FAILURE;
        }
        default:
        {
            /* Catch all for formats not supported yet */
            return SYS_FAILURE;
        }
    }

}

static sys_result_e rgb888_convert(uint32_t srcFmt, const uint8_t *src_frame, uint8_t *dest_frame)
{
    uint32_t i = 0;
    uint32_t j = 0;
    int y_temp, y2_temp, u_temp, v_temp;
    switch (srcFmt)
    {
        case V4L2_PIX_FMT_YUYV:
        {
            for (i = 0, j = 0; i < (FRAME_SIZE); i = i + 4, j = j + 6)
            {
                /* Minor optimization with loop unrolling */
                y_temp  = (int)src_frame[i + 0];
                u_temp  = (int)src_frame[i + 1];
                y2_temp = (int)src_frame[i + 2];
                v_temp  = (int)src_frame[i + 3];
                yuv2rgb(y_temp, u_temp, v_temp, &dest_frame[j], &dest_frame[j + 1], &dest_frame[j + 2]);
                yuv2rgb(y2_temp, u_temp, v_temp, &dest_frame[j + 3], &dest_frame[j + 4], &dest_frame[j + 5]);
            }
            break;
        }
        case V4L2_PIX_FMT_MJPEG:
        case V4L2_PIX_FMT_RGB888:
        {
            /* Already in RGB888 format */
            memcpy(dest_frame, src_frame, FRAME_SIZE);
            break;
        }
        default:
        {
            /* Catch all for formats not supported yet */
            return SYS_FAILURE;
        }
    }

    return SYS_SUCCESS;
}

static sys_result_e mjpeg_convert(uint32_t srcFmt, const uint8_t *src_frame, uint8_t *dest_frame)
{
    return SYS_SUCCESS;
}

static inline void yuv2rgb(int y, int u, int v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    int RR = CoefficientsY[y] + CoeffecientsRV[v];
    int GG = CoefficientsY[y] + CoeffecientsGU[u] + CoeffecientsGV[v];
    int BB = CoefficientsY[y] + CoeffecientsBU[u];

    RR /= PRECISION;
    GG /= PRECISION;
    BB /= PRECISION;


    // Computed values may need clipping.
    CLAMP_UPPER(RR, 255);
    CLAMP_LOWER(RR, 0);

    CLAMP_UPPER(GG, 255);
    CLAMP_LOWER(GG, 0);

    CLAMP_UPPER(BB, 255);
    CLAMP_LOWER(BB, 0);


    *r = (uint8_t) RR & 0xFFu;
    *g = (uint8_t) GG & 0xFFu;
    *b = (uint8_t) BB & 0xFFu;
}

#ifdef STREAM_TO_SERVER
sys_result_e image_save(const rgb_frame_t *buf, const uint32_t size)
{
    if (sockfd == -1)
    {
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Socket creation failed");
            return SYS_FAILURE;
        }
    
        memset(&pc_addr, 0, sizeof(pc_addr));
    
        // Fill PC information
        pc_addr.sin_family = AF_INET;
        pc_addr.sin_port = htons(3000);
        pc_addr.sin_addr.s_addr = inet_addr("192.168.7.1");
    }

    // Send MJPEG data
    ssize_t wBytes = 0;
    ssize_t sizeBuf = size;
    ssize_t sizeSent = 0;
    do
    {
        // UDP can only send 65,535 bytes at a time so we need to split the data
        CLAMP_UPPER(sizeBuf, 1024);
        sizeSent = sendto(sockfd, (char*)buf + wBytes, sizeBuf, 0, (const struct sockaddr *)&pc_addr, sizeof(pc_addr));
        if (sizeSent < 0)
        {
            perror("Send failed");
            return SYS_FAILURE;
        }

        wBytes += sizeSent;
        sizeBuf = size - wBytes;
    } while (wBytes < size);

    return SYS_SUCCESS;
}
#else
sys_result_e image_save(const rgb_frame_t *buf, const uint32_t size)
{
    char out_name[256];
    int ret;

    sprintf(out_name, IMAGE_FILE("/media/card/pics/frame%04d"), frameIdx);
    int file = open(out_name, O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK | O_NDELAY);
    if (0 > file)
    {
        SYS_TRACE("ERR: FILE OPEN");
        return SYS_FAILURE;
    }

    ret = file_write_blocking(file, buf, size);
    if (ret < 0)
    {
        SYS_TRACE("ERR: WRITE %s", strerror(errno));
        return SYS_FAILURE;
    }

    frameIdx++;

    close(file);

    return SYS_SUCCESS;
}
#endif

uint32_t image_getsavedframes(void)
{
    return frameIdx;
}

static int file_write_blocking(int fd, const void *buf, size_t size)
{
    /* Bytes written to file */
    ssize_t wBytes = 0;

    size_t sizeBuf = size;

    do
    {
        wBytes += write(fd, (char*)buf + wBytes, sizeBuf);
        sizeBuf -= (size_t)wBytes;

    } while (wBytes < size);

    return wBytes;
}

sys_result_e image_close(void)
{
    close(sockfd);
    imagebuffer_initialized = false;

    return SYS_SUCCESS;
}