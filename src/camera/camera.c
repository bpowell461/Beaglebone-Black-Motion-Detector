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

/* TASK RATE: 1Hz */ 
#define TASK_RATE_MSEC  (1 * MSEC_PER_SEC)

/** Type Definitions **/
typedef enum
{
    eCAMERA_ON,
    eCAMERA_OFF,
    eCAMERA_COUNT
}camera_state_e;

/** Static Variables **/
static const UINT32 task_rate_msec = TASK_RATE_MSEC;
static INT32 camera_fd;

static char *dev_name = "/dev/video0";
static UINT32 width = PIXEL_WIDTH;
static UINT32 height = PIXEL_HEIGHT;

/** Global Variables **/

/** Internal Function Prototypes **/
static sys_result_e camera_capturestate(camera_state_e state);
static sys_result_e camera_ioctl(int fh, int request, void *arg);


void camera_init(INT32 *fd)
{
    /* V4L2 Format Vars */
    struct v4l2_format  fmt;

    camera_fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if (camera_fd < 0) {
        SYS_TRACE("ERR: Cannot open device");
        exit(EXIT_FAILURE);
    }

    /* Setting camera format  */
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = PIXEL_FORMAT_CAMERA;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    camera_ioctl(camera_fd, VIDIOC_S_FMT, &fmt);
    if (fmt.fmt.pix.pixelformat != PIXEL_FORMAT_CAMERA)
    {
        SYS_TRACE("ERR: Libv4l didn't accept format. Can't proceed.\n");
        exit(EXIT_FAILURE);
    }

    if ((fmt.fmt.pix.width != 640) || (fmt.fmt.pix.height != 480))
        SYS_TRACE("WARN: driver is sending image at %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    *fd = camera_fd;
}

void *camera_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Camera Task Waiting for Start...");

    osal_task_set_period(id, task_rate_msec);

    osal_task_wait_start(id);

    camera_capturestate(eCAMERA_ON);

    while(DEF_TRUE)
    {
        if (SAVED_FRAMES_MAX <= framebuffer_getframeidx())
        {
            break;
        }

        SYS_TRACE("Writing frame");
        if (SYS_SUCCESS != framebuffer_writeframe(camera_fd))
        {
            SYS_TRACE("ERR: WRITEFRAME");
        }

        osal_task_delay(id);
    }

    SYS_TRACE("Camera Task Exiting...");
    camera_capturestate(eCAMERA_OFF);
    osal_task_delete(id);

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

static sys_result_e camera_ioctl(int fh, int request, void *arg)
{
    if (-1 == ioctl(fh, request, arg))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

