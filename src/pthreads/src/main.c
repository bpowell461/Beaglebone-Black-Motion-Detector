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

void* bootstrapper_task(void* threadp);

static osal_id_t bootstrap_id;

int main(void)
{
    INT32 v4l_fd;
    syslog_init(NAME, COURSE, ASSIGNMENT);
    syslog_printheader();

    camera_init(&v4l_fd);
    framebuffer_init(&v4l_fd, 2);
    nvm_init(&v4l_fd);
    
    osal_init();

    osal_priority_t prio = { 98, 0 };

    osal_task_create(&bootstrap_id, "bootstrap", 0, prio, bootstrapper_task, NULL);
 
    return 0;
}

void* bootstrapper_task(void* threadp)
{
    osal_priority_t prio = {97, 0};
    osal_id_t camera_id;
    osal_task_create(&camera_id, "camera", 0, prio, camera_task, NULL);
    osal_task_start(camera_id);

    prio = (osal_priority_t){96, 0};
    osal_id_t nvm_id;
    osal_task_create(&nvm_id, "nvm", 0, prio, nvm_task, NULL);
    osal_task_start(nvm_id);

    osal_task_delete(bootstrap_id);

    return NULL;
}