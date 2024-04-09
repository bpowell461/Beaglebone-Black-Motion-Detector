#include "camera.h"
#include "ringbuffer.h"
#include "osal_cfg.h"

#define TASK_HZ (1U)

ringbuffer_typedef(int, frameBuffer_t);
static frameBuffer_t frameBuffer;

void camera_init(void)
{
    ringbuffer_init(frameBuffer, int, 4);
}

void *camera_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    osal_wait_start(id);

    while(DEF_TRUE)
    {
        
    }
}