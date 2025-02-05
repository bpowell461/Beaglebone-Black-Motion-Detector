#include "image.h"
#include "camera_cfg.h"
#include "ringbuffer.h"
#include "osal.h"
#include "framebuffer.h"
#include "transcoder.h"
#include "syslog.h"

static void process_frames(void);

static int camera_fd;
static uint8_t exit_task = false;

void transcoder_init(int *fd)
{
    imagebuffer_init();

    camera_fd = *fd;
}

void *transcoder_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Transcoder Task (ID: %u) Waiting for Start...", id);

    osal_task_wait_start(id);

    while (true)
    {
        for (uint8_t i = 0; i < 2; i++)
        {
            process_frames();

            if (image_getsavedframes() >= SAVED_FRAMES_MAX)
            {
                exit_task = true;
                break;
            }
        }

        if (exit_task)
        {
            SYS_TRACE("Maximum frames saved reached");
            break;
        }
        else
        {
            osal_task_delay(id);
        }
    }

    SYS_TRACE("Transcoder Task Exiting...");

    osal_task_delete(id, false);

    return NULL;
}

static void process_frames(void)
{
    rgb_frame_t rgbFrame;
    frame_t     *rawFrame = NULL;

    if ( SYS_SUCCESS == framebuffer_getframe_ptr(camera_fd, &rawFrame))
    {
        image_convert(V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_RGB888, rawFrame->bytes, rgbFrame.bytes);

        rgbFrame.timestamp = rawFrame->timestamp;

        imagebuffer_write(&rgbFrame);

        framebuffer_freeframe(camera_fd, rawFrame);
    }
}
