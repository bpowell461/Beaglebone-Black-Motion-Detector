#ifndef CAMERA_CFG_H_
#define CAMERA_CFG_H_

#include <linux/videodev2.h>
#include "utils.h"

#define CAMERA_USE_PPM
#define SAVED_FRAMES_MAX    16
#define CAMERA_ACQUISITION_10HZ

#define MAX_IGNORE_FRAMES (100u)

#if defined(CAMERA_ACQUISITION_1HZ)
#define OVERSAMPLE_FRAME 20
#elif defined(CAMERA_ACQUISITION_10HZ)
#define OVERSAMPLE_FRAME 2
#else
#error "No acquisition mode defined"
#endif

#if defined(CAMERA_USE_YUV)
#define PIXEL_FORMAT_CAMERA (V4L2_PIX_FMT_YUYV)
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_NONE)
#define PIXEL_WIDTH     640
#define PIXEL_HEIGHT    480
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
#define IMAGE_EXT       ".jpg"

#else

#error "No image format specified"

#endif

#define IMAGE_FILE(x) (x IMAGE_EXT)
#define IMAGE_HEADER ("P6\n# %s\n# Time (ms): %ld \n"STR(PIXEL_WIDTH)" "STR(PIXEL_HEIGHT)"\n255\n")
#define RGB_FRAME_SIZE_BYTES (PIXEL_WIDTH * PIXEL_HEIGHT * 3)

/* Custom Image Formats */
#define V4L2_PIX_FMT_RGB888 (v4l2_fourcc('R', 'G', 'B', '8'))

/* This is the "raw" frame size */
typedef struct
{
    struct timespec timestamp;
    UINT08 bytes[FRAME_SIZE];
}frame_t;

/* Modified frame size using RGB888 */
typedef struct
{
    struct timespec timestamp;
    UINT08 bytes[RGB_FRAME_SIZE_BYTES];
}rgb_frame_t;

#endif
