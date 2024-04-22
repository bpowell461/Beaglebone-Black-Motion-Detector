#include "image.h"
#include "camera_cfg.h"
#include "ringbuffer.h"
#include "osal.h"
#include "framebuffer.h"
#include "transcoder.h"
#include "syslog.h"

ringbuffer_typedef(frame_t, rawImageBuffer_t);

static rawImageBuffer_t incomingBuffer;

static void read_frames(void);
static void process_frames(void);

static INT32 camera_fd;


void transcoder_init(INT32 *fd)
{
    ringbuffer_init(incomingBuffer, frame_t, 8);
    imagebuffer_init();

    camera_fd = *fd;
}

void *transcoder_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Transcoder Task (ID: %u) Waiting for Start...", id);

    osal_task_wait_start(id);

    while (DEF_TRUE)
    {
        if (image_getsavedframes() >= SAVED_FRAMES_MAX)
        {
            break;
        }
        read_frames();

        process_frames();

        osal_task_delay(id);
    }

    SYS_TRACE("Transcoder Task Exiting...");

    osal_task_delete(id, DEF_FALSE);

    return NULL;
}

static void read_frames(void)
{
    UINT32 *framePtr = DEF_NULL_PTR;
    if (SYS_SUCCESS == framebuffer_getframe(camera_fd, framePtr))
    {
        if (!ringbuffer_isFull(&incomingBuffer))
        {
            ringbuffer_write(&incomingBuffer, *(frame_t *)framePtr);
        }
    }
}
static void process_frames(void)
{
    rgb_frame_t rgbFrame;
    frame_t     *rawFrame;
    if (!ringbuffer_isEmpty(&incomingBuffer))
    {
        ringbuffer_read_zc(&incomingBuffer, rawFrame);

        image_convert(V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_RGB888, rawFrame->bytes, rgbFrame.bytes);

        imagebuffer_write(&rgbFrame);

        ringbuffer_inc_readptr(&incomingBuffer);
    }
}
