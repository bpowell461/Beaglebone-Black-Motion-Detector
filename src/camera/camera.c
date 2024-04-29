#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include "camera.h"
#include "osal.h"
#include "utils.h"
#include "syslog.h"
#include "framebuffer.h"
#include "camera_cfg.h"

/** Macros **/
#define MAX_WRITE_ERRORS (5u)
#define MAX_IGNORE_FRAMES (8u)

/** Type Definitions **/
typedef enum
{
    eCAMERA_ON,
    eCAMERA_OFF,
    eCAMERA_COUNT
}camera_state_e;

typedef enum
{
    eSTATE_ADJUSTING,
    eSTATE_CAPTURING,
    eSTATE_EXIT,
    eSTATE_COUNT
}camera_fsm_e;

typedef enum
{
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR
}v4l2_io_e;

/** Static Variables **/
static INT32 camera_fd;
static UINT08 num_writes = 0;
static UINT08 ignoreFrames = 0;

static char *dev_name = "/dev/video0";

/** Global Variables **/

/** Internal Function Prototypes **/
static sys_result_e camera_capturestate(camera_state_e state);
static sys_result_e camera_ioctl(INT32 fh, UINT32 request, void *arg);


void camera_init(INT32 *fd)
{
    /* V4L2 Format Vars */
    struct v4l2_format  fmt;
    struct v4l2_capability cap;
    v4l2_io_e io = IO_METHOD_MMAP;

    camera_fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (camera_fd < 0) {
        SYS_TRACE("ERR: Cannot open device");
        exit(EXIT_FAILURE);
    }

    camera_ioctl(camera_fd, VIDIOC_QUERYCAP, &cap);

    switch (io)
    {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE))
            {
                SYS_TRACE("%s does not support read i/o\n", dev_name);
                exit(EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING))
            {
                SYS_TRACE("%s does not support streaming i/o\n", dev_name);
                exit(EXIT_FAILURE);
            }
            break;
    }

    /* Setting camera format  */
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = PIXEL_WIDTH;
    fmt.fmt.pix.height = PIXEL_HEIGHT;
    fmt.fmt.pix.pixelformat = PIXEL_FORMAT_CAMERA;
    fmt.fmt.pix.field = PIXEL_FORMAT_FIELD;

    camera_ioctl(camera_fd, VIDIOC_S_FMT, &fmt);
    if (fmt.fmt.pix.pixelformat != PIXEL_FORMAT_CAMERA)
    {
        SYS_TRACE("ERR: Libv4l didn't accept format. Can't proceed.\n");
        exit(EXIT_FAILURE);
    }

    if ((fmt.fmt.pix.width != PIXEL_WIDTH) || (fmt.fmt.pix.height != PIXEL_HEIGHT))
        SYS_TRACE("WARN: driver is sending image at %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (SYS_SUCCESS != camera_ioctl(camera_fd, VIDIOC_G_PARM, &streamparm))
    {
        SYS_TRACE("ERR: Getting VIDIOC parameters");
        exit(EXIT_FAILURE);
    }

    streamparm.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 30;
    if (SYS_SUCCESS != camera_ioctl(camera_fd, VIDIOC_S_PARM, &streamparm))
    {
        SYS_TRACE("ERR: Setting VIDIOC parameters");
        exit(EXIT_FAILURE);
    }

    *fd = camera_fd;

}

void *camera_task(void *threadp)
{
    camera_fsm_e state = eSTATE_ADJUSTING;
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Camera Task (ID: %u) Waiting for Start...", id);

    osal_task_wait_start(id);

    camera_capturestate(eCAMERA_ON);

    while(DEF_TRUE)
    {
        switch (state)
        {
            case eSTATE_ADJUSTING:
            {
                if (SYS_FAILURE != framebuffer_writeframe(camera_fd, DEF_FALSE))
                {
                    SYS_TRACE("Ignore frame");
                    ignoreFrames++;
                }
                if (ignoreFrames >= MAX_IGNORE_FRAMES)
                {
                    state = eSTATE_CAPTURING;
                }
                break;
            }
            case eSTATE_CAPTURING:
            {
                if (SYS_SUCCESS == framebuffer_writeframe(camera_fd, DEF_TRUE))
                {
                    SYS_TRACE("Writing frame: %u", num_writes);
                    num_writes++;
                }
                if (SAVED_FRAMES_MAX <= num_writes)
                {
                    state = eSTATE_EXIT;
                }
                break;
            }
            case eSTATE_EXIT:
            {
                SYS_TRACE("Camera Task Exiting...");
                camera_capturestate(eCAMERA_OFF);
                osal_task_delete(id, DEF_FALSE);
                break;
            }
            default:
            {
                state = eSTATE_EXIT;
                break;
            }
        }

        osal_task_delay(id);
    }

    return NULL;
}

static sys_result_e camera_capturestate(camera_state_e state)
{
    INT32 res;
    UINT32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (eCAMERA_ON == state)
    {
        SYS_TRACE("CAMERA STATE: ON");
        res = camera_ioctl(camera_fd, VIDIOC_STREAMON, &type);
    }
    else if (eCAMERA_OFF == state)
    {
        SYS_TRACE("CAMERA STATE: OFF");
        res = camera_ioctl(camera_fd, VIDIOC_STREAMOFF, &type);
    }
    else
    {
        res = SYS_FAILURE;
    }

    return res;
}

static sys_result_e camera_ioctl(INT32 fh, UINT32 request, void *arg)
{
    if (-1 == ioctl(fh, request, arg))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

