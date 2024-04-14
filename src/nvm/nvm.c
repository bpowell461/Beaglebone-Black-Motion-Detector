#include "nvm.h"
#include "ringbuffer.h"
#include "osal.h"
#include "framebuffer.h"
#include "utils.h"
#include "syslog.h"

/** Macros **/

/** Type Definitions **/

/** Static Variables **/
static INT32 nvm_fd;

/** Global Variables **/

/** Internal Function Prototypes **/

void nvm_init(INT32 *fd)
{
    nvm_fd = *fd;
}

void *nvm_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)&threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("NVM Task Waiting for Start...");

    osal_task_wait_start(id);

    while(DEF_TRUE)
    {
        SYS_TRACE("Getting frame");
        if (SYS_SUCCESS != framebuffer_getframe(nvm_fd))
        {
            SYS_TRACE("ERR: GET FRAME");
        }

        if (framebuffer_getframeidx() >= SAVED_FRAMES_MAX)
        {
            break;
        }
        osal_task_delay(id);
    }

    SYS_TRACE("NVM Task Exiting...");
    framebuffer_deinit();

    osal_task_delete(id, DEF_FALSE);

    return NULL;
} 