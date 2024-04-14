#include "nvm.h"
#include "ringbuffer.h"
#include "osal.h"
#include "framebuffer.h"
#include "utils.h"

/** Macros **/
#define TASK_RATE_HZ        (1U)
#define TASK_RATE_USEC      ((1/TASK_RATE_HZ) * USEC_PER_MSEC * MSEC_PER_SEC)

/** Type Definitions **/

/** Static Variables **/
static const UINT32 task_rate_usec = TASK_RATE_USEC * 3;
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

    osal_task_wait_start(id);

    while(DEF_TRUE)
    {
        framebuffer_getframe(nvm_fd);

        if (framebuffer_getframeidx() >= SAVED_FRAMES_MAX)
        {
            break;
        }
        osal_task_delay(task_rate_usec);
    }

    framebuffer_deinit();

    osal_task_delete(id);

    return NULL;
} 