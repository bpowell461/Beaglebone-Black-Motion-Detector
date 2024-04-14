/* Custom Includes */
#include "syslog.h"
#include "types.h"
#include "osal.h"
#include "camera.h"
#include "nvm.h"
#include "framebuffer.h"

#define NUM_THREADS 2

#define ASSIGNMENT 1
#define COURSE 3
#define NAME "capture"

int main(void)
{
    INT32 v4l_fd;
    syslog_init(NAME, COURSE, ASSIGNMENT);
    syslog_printheader();

    camera_init(&v4l_fd);
    framebuffer_init(&v4l_fd);
    nvm_init(&v4l_fd);
    
    if (SYS_SUCCESS != osal_init())
    {
        SYS_TRACE("ERR: OS_INITIALIZE");
        return 1;
    }

    SYS_TRACE("OS INITIALIZED");

    osal_priority_t prio = { 90, 0 };
    osal_id_t camera_id;
    if (SYS_SUCCESS != osal_task_create(&camera_id, "camera", 0, prio, camera_task, NULL))
    {
        SYS_TRACE("ERR: CAMERA TASK CREATE");
    }

    prio = (osal_priority_t){ 80, 0 };
    osal_id_t nvm_id;
    if (SYS_SUCCESS != osal_task_create(&nvm_id, "nvm", 0, prio, nvm_task, NULL))
    {
        SYS_TRACE("ERR: NVM TASK CREATE");
    }

    osal_start_scheduler();

    UINT32 tasksCount = osal_task_wait_all();

    SYS_TRACE("%u tasks completed.", tasksCount);

    osal_deinit();
 
    return 0;
}