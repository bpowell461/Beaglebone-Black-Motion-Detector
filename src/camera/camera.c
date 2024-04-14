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

/** Macros **/
#define TASK_RATE_HZ    (1U)
#define TASK_RATE_USEC  ((1/TASK_RATE_HZ) * USEC_PER_MSEC * MSEC_PER_SEC)

/** Type Definitions **/
typedef enum
{
    eCAMERA_ON,
    eCAMERA_OFF,
    eCAMERA_COUNT
}camera_state_e;

/** Static Variables **/
static const UINT32 task_rate_usec = TASK_RATE_USEC;
static INT32 camera_fd;

static char *dev_name = "/dev/video0";
static UINT32 width = 640;
static UINT32 height = 480;

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
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    camera_ioctl(camera_fd, VIDIOC_S_FMT, &fmt);
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24)
    {
        SYS_TRACE("ERR: Libv4l didn't accept RGB24 format. Can't proceed.\n");
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

    osal_task_wait_start(id);

    camera_capturestate(eCAMERA_ON);

    while(DEF_TRUE)
    {
        if (SAVED_FRAMES_MAX <= framebuffer_getframeidx())
        {
            break;
        }
        else
        {
            framebuffer_writeframe(camera_fd);
        }

        osal_task_delay(task_rate_usec);
    }

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
        res = camera_ioctl(camera_fd, VIDIOC_STREAMON, &type);
    }
    else if (eCAMERA_OFF == state)
    {
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

