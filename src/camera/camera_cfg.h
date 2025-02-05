#ifndef CAMERA_CFG_H_
#define CAMERA_CFG_H_

#include <linux/videodev2.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include "utils.h"

/* Application Specific Defines */
#define CAMERA_USE_MJPEG

/* Configuration Defines */
#define MAX_IGNORE_FRAMES (100u)

#if defined(CAMERA_USE_YUV)
#define PIXEL_FORMAT_CAMERA (V4L2_PIX_FMT_YUYV)
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_NONE)
#define PIXEL_WIDTH     640
#define PIXEL_HEIGHT    480
#define FRAME_SIZE      (PIXEL_WIDTH * PIXEL_HEIGHT)
#define IMAGE_EXT       ".yuv"

#elif defined(CAMERA_USE_PPM)

#define PIXEL_FORMAT_CAMERA (V4L2_PIX_FMT_YUYV)
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_NONE)
#define PIXEL_WIDTH     320
#define PIXEL_HEIGHT    240
#define FRAME_SIZE      (PIXEL_WIDTH * PIXEL_HEIGHT * 2)
#define IMAGE_EXT       ".ppm"

#elif defined(CAMERA_USE_MJPEG)

#define PIXEL_FORMAT_CAMERA v4l2_fourcc('M', 'J', 'P', 'G')
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_INTERLACED)
#define PIXEL_WIDTH     640
#define PIXEL_HEIGHT    480
#define FRAME_SIZE      (PIXEL_WIDTH * PIXEL_HEIGHT)
#define IMAGE_EXT       ".mjpg"

#else

#error "No image format specified"

#endif

#define IMAGE_FILE(x) (x IMAGE_EXT)
#define RGB_FRAME_SIZE_BYTES (PIXEL_WIDTH * PIXEL_HEIGHT * 3)

/* Custom Image Formats */
#define V4L2_PIX_FMT_RGB888 (v4l2_fourcc('R', 'G', 'B', '8'))

#pragma pack(1)
/* This is the "raw" frame size */
typedef struct
{
    uint8_t bytes[FRAME_SIZE];
}frame_t;

/* Modified frame size using RGB888 */
typedef struct
{
    uint8_t bytes[RGB_FRAME_SIZE_BYTES];
}rgb_frame_t;
#pragma pack(pop)

#endif
