#include "nvm.h"
#include "osal.h"
#include "utils.h"
#include "camera_cfg.h"
#include "image.h"
#include "syslog.h"

/** Macros **/
#define SAVE_ITERATIONS (1u)

/** Type Definitions **/

/** Static Variables **/
static INT32 nvm_fd;
static BOOL_T exit_task = DEF_FALSE;
static UINT08 saved_frames = 0;

/** Global Variables **/

/** Internal Function Prototypes **/

void nvm_init(INT32 *fd)
{
    nvm_fd = *fd;
    imagebuffer_init();
}

void *nvm_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    rgb_frame_t *save_frame = DEF_NULL_PTR;

    SYS_TRACE("NVM Task (ID: %u) Waiting for Start...", id);

    osal_task_wait_start(id);

    while(DEF_TRUE)
    {
        SYS_TRACE("Getting frame(s)");

        if (SYS_SUCCESS == imagebuffer_startread(save_frame))
        {
            SYS_TRACE("Found frame");
            if (SYS_SUCCESS != image_save(save_frame->bytes, RGB_FRAME_SIZE_BYTES))
            {
                SYS_TRACE("ERR: SAVING FILE");
            }
            imagebuffer_endread();

            if (image_getsavedframes() >= SAVED_FRAMES_MAX)
            {
                exit_task = DEF_TRUE;
                break;
            }
        }
        else
        {
            SYS_TRACE("No frames found. Sleeping...");
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

    SYS_TRACE("NVM Task Exiting...");

    osal_task_delete(id, DEF_FALSE);

    return NULL;
} 