/* Custom Includes */
#include "syslog.h"
#include "types.h"
#include "osal.h"
#include "camera.h"
#include "nvm.h"
#include "framebuffer.h"

#define NUM_THREADS 2
#define NUM_FRAME_BUFS 5

#define ASSIGNMENT 1
#define COURSE 3
#define NAME "capture"

void* bootstrapper_task(void* threadp);

static osal_id_t bootstrap_id;

int main(void)
{
    INT32 v4l_fd;
    syslog_init(NAME, COURSE, ASSIGNMENT);
    syslog_printheader();

    camera_init(&v4l_fd);
    framebuffer_init(&v4l_fd, NUM_FRAME_BUFS);
    nvm_init(&v4l_fd);
    
    if (SYS_SUCCESS != osal_init())
    {
        SYS_TRACE("ERR: OS_INITIALIZE");
        return 1;
    }

    SYS_TRACE("OS INITIALIZED");

    osal_priority_t prio = { 98, 0 };

    if (SYS_SUCCESS != osal_task_create(&bootstrap_id, "bootstrap", 0, prio, bootstrapper_task, NULL))
    {
        SYS_TRACE("ERR: BOOTSTRAP TASK CREATE");
        return 1;
    }

    if (SYS_SUCCESS != osal_task_wait_id(bootstrap_id))
    {
        SYS_TRACE("ERR: WAIT ID (BOOTSTRAP)");
    }

    osal_deinit();
 
    return 0;
}

void* bootstrapper_task(void* threadp)
{
    osal_task_start(bootstrap_id);
    osal_task_wait_start(bootstrap_id);

    osal_priority_t prio = {90, 0};
    osal_id_t camera_id;
    if (SYS_SUCCESS != osal_task_create(&camera_id, "camera", 0, prio, camera_task, NULL))
    {
        SYS_TRACE("ERR: CAMERA TASK CREATE");
    }

    prio = (osal_priority_t){80, 0};
    osal_id_t nvm_id;
    if (SYS_SUCCESS != osal_task_create(&nvm_id, "nvm", 0, prio, nvm_task, NULL))
    {
        SYS_TRACE("ERR: NVM TASK CREATE");
    }

    osal_task_start(camera_id);
    osal_task_start(nvm_id);

    UINT32 tasksCount = osal_task_wait_all();

    SYS_TRACE("BOOTSTRAPPER: Waiting on %u tasks to complete...", tasksCount);

    osal_task_delete(bootstrap_id, DEF_FALSE);

    return NULL;
}