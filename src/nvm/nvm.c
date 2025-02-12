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
static int nvm_fd;
static uint8_t exit_task = false;

/** Global Variables **/

/** Internal Function Prototypes **/

void nvm_init(int *fd)
{
    nvm_fd = *fd;
    imagebuffer_init();
}

void *nvm_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    rgb_frame_t *save_frame = NULL;

    SYS_TRACE("NVM Task (ID: %u) Starting...", id);

    while(true)
    {
        for (uint8_t i = 0; i < 2; i ++)
        {
            if (SYS_SUCCESS == imagebuffer_startread(&save_frame))
            {
                if (SYS_SUCCESS != image_save(save_frame, RGB_FRAME_SIZE_BYTES))
                {
                    SYS_TRACE("ERR: SAVING FILE");
                }
                
                imagebuffer_endread();
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

    SYS_TRACE("NVM Task Exiting...");

    image_close();
    osal_task_delete(id, false);

    return NULL;
} 

void *nvm_exit(void *threadp)
{
    exit_task = true;

    return NULL;
}