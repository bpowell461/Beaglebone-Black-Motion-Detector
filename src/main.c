/* Custom Includes */
#include "syslog.h"
#include "types.h"
#include "osal.h"
#include "camera.h"
#include "nvm.h"
#include "framebuffer.h"
#include "transcoder.h"

#define NUM_THREADS 2

#define ASSIGNMENT 1
#define COURSE 4
#define NAME "Final Project"

#ifdef SIMPLE_TASKS
static void *HelloDog_Task(void *threadp);
static void *HelloCat_Task(void *threadp);
#endif // SIMPLE_TASKS

int main(void)
{
    INT32 v4l_fd;
    syslog_init(NAME, COURSE, ASSIGNMENT);
    syslog_printheader();

    if (SYS_SUCCESS != osal_init())
    {
        SYS_TRACE("ERR: OS_INITIALIZE");
        return SYS_FAILURE;
    }

    SYS_TRACE("OS INITIALIZED");

#ifndef SIMPLE_TASKS

    camera_init(&v4l_fd);
    framebuffer_init(&v4l_fd);
    nvm_init(&v4l_fd);
    transcoder_init(&v4l_fd);

    osal_priority_t prio = 97;
    osal_id_t camera_id;
    if (SYS_SUCCESS != osal_task_create(&camera_id, "camera", 0, prio, camera_task, TASK_RATE_20HZ, NULL))
    {
        SYS_TRACE("ERR: CAMERA TASK CREATE");
    }

    prio = 80;
    osal_id_t transcoder_id;
    if (SYS_SUCCESS != osal_task_create(&transcoder_id, "transcoder", 0, prio, transcoder_task, (TASK_RATE_10HZ), NULL))
    {
        SYS_TRACE("ERR: NVM TASK CREATE");
    }

    prio = 75;
    osal_id_t nvm_id;
    if (SYS_SUCCESS != osal_task_create(&nvm_id, "nvm", 0, prio, nvm_task, (2 * TASK_RATE_10HZ), NULL))
    {
        SYS_TRACE("ERR: NVM TASK CREATE");
    }

#else
    osal_priority_t prio = 90;
    osal_id_t hellodog;
    if (SYS_SUCCESS != osal_task_create(&hellodog, "hellodog", 0, prio, HelloDog_Task, TASK_RATE_1HZ, NULL))
    {
        SYS_TRACE("ERR: HELLO DOG TASK CREATE");
    }

    prio = 80;
    osal_id_t hellocat;
    if (SYS_SUCCESS != osal_task_create(&hellocat, "hellocat", 0, prio, HelloCat_Task, (3 * TASK_RATE_1HZ), NULL))
    {
        SYS_TRACE("ERR: HELLO CAT TASK CREATE");
    }
#endif

    osal_start_scheduler();

    osal_task_wait_all();

    framebuffer_deinit();
    osal_deinit();
 
    return 0;
}

#ifdef SIMPLE_TASKS

static void *HelloDog_Task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Hello Dog Waiting for Start...");

    UINT32 count = 0;

    osal_task_wait_start(id);

    while (DEF_TRUE)
    {
        count++;
        if (count >= 5)
        {
            break;
        }

        SYS_TRACE("Hello Dog: %u", count);

        osal_task_delay(id);
    }

    SYS_TRACE("HelloDog_Task Exiting...");
    osal_task_delete(id, DEF_FALSE);

    return NULL;
}

static void *HelloCat_Task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Hello Cat Waiting for Start...");

    UINT32 count = 0;

    osal_task_wait_start(id);

    while (DEF_TRUE)
    {
        count++;
        if (count >= 5)
        {
            break;
        }

        SYS_TRACE("Hello Cat: %u", count);

        osal_task_delay(id);
    }

    SYS_TRACE("Hello Cat Task Exiting...");
    osal_task_delete(id, DEF_FALSE);

    return NULL;
}
#endif // SIMPLE_TASKS