/* Custom Includes */
#include "syslog.h"
#include "types.h"
#include "osal.h"
#include "camera.h"
#include "nvm.h"

#define NUM_THREADS 2

#define ASSIGNMENT 1
#define COURSE 3
#define NAME "capture"

void* bootstrapper_task(void* threadp);

typedef void* (*threadFunc_t)(void*);

int main(void)
{
    syslog_init(NAME, COURSE, ASSIGNMENT);
    syslog_printheader();

    camera_init();
    nvm_init();
    
 
    return 0;
}

void* bootstrapper_task(void* threadp)
{

}