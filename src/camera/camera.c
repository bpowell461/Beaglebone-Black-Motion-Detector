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
#include "events.h"

/** Macros **/

/** Type Definitions **/
typedef enum
{
    eCAMERA_ON,
    eCAMERA_OFF,
    eCAMERA_COUNT
}camera_state_e;

typedef enum
{
    eSTATE_IDLE,
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
static int camera_fd;
static uint32_t num_writes = 0;
static uint32_t ignoreFrames = 0;
static camera_fsm_e state = eSTATE_IDLE;
static osal_mqueue_t mqueue;

static char *dev_name = "/dev/video0";

/** Global Variables **/

/** Internal Function Prototypes **/
static sys_result_e camera_capturestate(camera_state_e state);
static sys_result_e camera_ioctl(int fh, uint32_t request, void *arg);
static event_e process_event_queue(void);
static void set_camera_state(camera_fsm_e new_state);

void camera_init(int *fd)
{
    /* V4L2 Format Vars */
    struct v4l2_format  fmt;
    struct v4l2_control ctrl;
    struct v4l2_streamparm streamparm;
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
    {
        SYS_TRACE("ERR: driver is sending image at %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
        exit(EXIT_FAILURE);
    }

    /* Setting camera framerate */
    CLEAR(streamparm);

    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 30;

    camera_ioctl(camera_fd, VIDIOC_S_PARM, &streamparm);
    camera_ioctl(camera_fd, VIDIOC_G_PARM, &streamparm);
    SYS_TRACE("Camera framerate: %u FPS", streamparm.parm.capture.timeperframe.denominator);

    /* Setting camera control */
    CLEAR(ctrl);

    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = V4L2_EXPOSURE_MANUAL;
    camera_ioctl(camera_fd, VIDIOC_S_CTRL, &ctrl);

    ctrl.id = V4L2_CID_FOCUS_AUTO;
    ctrl.value = false;
    camera_ioctl(camera_fd, VIDIOC_S_CTRL, &ctrl);

    ctrl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctrl.value = 0;
    camera_ioctl(camera_fd, VIDIOC_S_CTRL, &ctrl);

    *fd = camera_fd;

    osal_queue_create(&mqueue, EVENT_QUEUE_NAME, 10, sizeof(event_e));
}

void *camera_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Camera Task (ID: %u) Starting...", id);

    camera_capturestate(eCAMERA_ON);

    while(true)
    {
        event_e event = process_event_queue();
        
        switch (event)
        {
            case EVENT_MOTION_DETECTED:
            {
                if (state == eSTATE_IDLE)
                {
                    set_camera_state(eSTATE_CAPTURING);
                };
                break;
            }
            case EVENT_MOTION_LOST:
            {
                if (state != eSTATE_EXIT)
                {
                    set_camera_state(eSTATE_IDLE);
                }
                break;
            }
            default:
            {
                break;
            }
        }

        switch (state)
        {
            case eSTATE_IDLE:
            {
                // Lets not write the frame to the framebuffer, but still capture it
                framebuffer_writeframe(camera_fd, false);
                break;
            }
            case eSTATE_CAPTURING:
            {
                if (SYS_SUCCESS == framebuffer_writeframe(camera_fd, true))
                {
                    num_writes++;
                }
                break;
            }
            case eSTATE_EXIT:
            {
                SYS_TRACE("Camera Task Exiting...");
                camera_capturestate(eCAMERA_OFF);
                break;
            }
            default:
            {
                set_camera_state(eSTATE_IDLE);
                break;
            }
        }
        osal_task_delay(id);
    }

    osal_task_delete(id, false);
    return NULL;
}

void *camera_exit(void *threadp)
{
    (void)threadp;
    set_camera_state(eSTATE_EXIT);
    return NULL;
}

static sys_result_e camera_capturestate(camera_state_e state)
{
    int res;
    uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

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

static sys_result_e camera_ioctl(int fh, uint32_t request, void *arg)
{
    if (-1 == ioctl(fh, request, arg))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

static event_e process_event_queue(void)
{
    event_e event = EVENT_NONE;
    if (SYS_SUCCESS != osal_queue_receive(&mqueue, &event, sizeof(event_e)))
    {
        return EVENT_NONE;
    }

    return event;
}

static void set_camera_state(camera_fsm_e new_state)
{
    SYS_TRACE("Camera FSM: %d", new_state);
    state = new_state;
}



