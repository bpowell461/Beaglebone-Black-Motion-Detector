#ifndef CAMERA_CFG_H_
#define CAMERA_CFG_H_

#include <linux/videodev2.h>

#define CAMERA_USE_PPM

#if defined(CAMERA_USE_YUV)
#define PIXEL_FORMAT_CAMERA (V4L2_PIX_FMT_YUYV)
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_NONE)
#define PIXEL_WIDTH     (640)
#define PIXEL_HEIGHT    (480)
#define IMAGE_EXT       ".yuv"

#elif defined(CAMERA_USE_PPM)

#define PIXEL_FORMAT_CAMERA (V4L2_PIX_FMT_YUYV)
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_INTERLACED)
#define PIXEL_WIDTH     (640)
#define PIXEL_HEIGHT    (480)
#define PIXEL_BYTES     (614400U)
#define IMAGE_EXT       ".raw"

#elif defined(CAMERA_USE_MJPEG)

#define PIXEL_FORMAT_CAMERA v4l2_fourcc('M', 'J', 'P', 'G')
#define PIXEL_FORMAT_FIELD  (V4L2_FIELD_INTERLACED)
#define PIXEL_WIDTH     (640)
#define PIXEL_HEIGHT    (480)
#define IMAGE_EXT       ".mjpg"

#else

#error "No image format specified"

#endif

#define IMAGE_FILE(x) (x IMAGE_EXT)

#endif
