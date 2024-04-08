/* Custom Includes */
#include "syslog.h"
#include "types.h"

/* Standard Includes */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>

#define NUM_THREADS 2

#define ASSIGNMENT 1
#define COURSE 3
#define NAME "capture"

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (1)

void* Bootstrapper_Task(void* threadp);
void* Camera_Task(void* threadp);
void* NVM_Task(void* threadp);

typedef void* (*threadFunc_t)(void*);

static pthread_t threads[NUM_THREADS];
static int rt_max_prio, rt_min_prio;
static struct sched_param rt_param[NUM_THREADS];

int main(void)
{
    int rc = 0;

    struct sched_param main_param;
    pid_t mainpid;

    syslog_init(NAME, COURSE, ASSIGNMENT);

    syslog_printheader();

    mainpid = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if (rc < 0) perror("main_param");
 
    return 0;
}

void* Bootstrapper_Task(void* threadp)
{
    osal_create;
}