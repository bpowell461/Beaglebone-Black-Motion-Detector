#include "nvm.h"
#include "ringbuffer.h"
#include "osal_cfg.h"
#include <linux/videodev2.h>
#include <libv4l2.h>

ringbuffer_typedef(int, frameBuffer_t);

static frameBuffer_t frameBuffer;

void nvm_init(void)
{
    ringbuffer_init(frameBuffer, int, 4);


}

void *nvm_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    osal_wait_start(id);

    for (;;)
    {
        
    }
} 