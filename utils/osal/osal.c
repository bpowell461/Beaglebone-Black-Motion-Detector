/**
 * @file osal.c
 * @author Brad Powell
 * @date 18 Mar 2024
 * @brief File containing definitions for Operating System Abstraction Layer functions.
 *
 */
#define _GNU_SOURCE
#include "osal.h"
#include "pthread.h"
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>

#define MAX_TASKS 128

#define CHECK_ERROR(x)  ((x) ? SYS_FAILURE : SYS_SUCCESS)

typedef struct
{
    osal_id_t       task_id;
    char*           task_name;
    osal_stack_t    task_stack;
    osal_priority_t task_priority;
    osal_func       task_func;
    void*           task_args;
    osal_task_t*    task_handle;
}osal_task_tcb_t;

static BOOL_T os_initialized = DEF_FALSE;

static osal_task_tcb_t osal_task_tcb[MAX_TASKS];
static osal_sem_t osal_task_signals[MAX_TASKS];

static UINT08 task_idx = 0;

static osal_mutex_t os_task_mutex;

static int rt_max_prio;
static int rt_min_prio;

sys_result_e osal_init(void)
{
    if (os_initialized)
        return SYS_SUCCESS;

    struct sched_param main_param;
    pid_t mainpid;
    int rc;

    mainpid = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO) - 1;
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio + 1;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    
    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    rc = osal_mutex_init(&os_task_mutex, OSAL_MTX_PRIO_CEILING);
    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

sys_result_e osal_task_create(osal_id_t *id, char* name, osal_stack_t stack, osal_priority_t priority, osal_func task_func, void* args)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int res;
    pthread_attr_t attr;
    cpu_set_t threadcpu;

    res = pthread_attr_init(&attr);
    if (CHECK_ERROR(res))
    {
        return SYS_FAILURE;
    }

    struct sched_param sp;
    res = pthread_attr_getschedparam(&attr, &sp);

    CPU_ZERO(&threadcpu);
    CPU_SET(0, &threadcpu);

    res = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    res = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    res = pthread_attr_setaffinity_np(&sp, sizeof(cpu_set_t), &threadcpu);

    sp.sched_priority = priority.priority;
    pthread_attr_setschedparam(&sp, &attr);

    /* CRITICAL SECTION */
    osal_mutex_lock(&os_task_mutex);

    *id = task_idx;
    osal_task_tcb[task_idx].task_id = *id;
    osal_task_tcb[task_idx].task_name = name;
    osal_task_tcb[task_idx].task_stack = stack;
    osal_task_tcb[task_idx].task_priority = priority;
    osal_task_tcb[task_idx].task_func = task_func;
    osal_task_tcb[task_idx].task_args = args;

    osal_sem_init(&osal_task_signals[task_idx]);

    res = pthread_create(&osal_task_tcb[task_idx].task_handle, &attr, task_func, args);
    if (!CHECK_ERROR(res))
    {
        pthread_join(osal_task_tcb[task_idx].task_handle, NULL);
        task_idx++;
    }

    osal_mutex_unlock(&os_task_mutex);
    /* END CRITICAL SECTION */

    pthread_attr_destroy(&attr);

    return SYS_SUCCESS;
}

sys_result_e osal_task_start(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = osal_sem_signal(&osal_task_signals[id]);

    return CHECK_ERROR(ret);
}

sys_result_e osal_task_delete(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;
}

sys_result_e osal_task_suspend(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = osal_sem_wait(&osal_task_signals[id]);

    return CHECK_ERROR(ret);
}

sys_result_e osal_buffer_put(void)
{
    if (!os_initialized)
        return SYS_FAILURE;
}

sys_result_e osal_mutex_init(osal_mutex_t *mutex, osal_mutex_attr_e attr)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = 0;

    pthread_mutexattr_t pthread_mtx;

    ret = pthread_mutexattr_init(&pthread_mtx);
    if (ret != SYS_SUCCESS)
    {
        return SYS_FAILURE;
    }

    switch (attr)
    {
        case OSAL_MTX_PRIO_INHERIT:
        {
            ret = pthread_mutexattr_setprotocol(&pthread_mtx, PTHREAD_PRIO_INHERIT);
            break;
        }
        case OSAL_MTX_PRIO_CEILING:
        {
            ret = pthread_mutexattr_setprioceiling(&pthread_mtx, rt_max_prio);
            pthread_mutexattr_setprotocol(&pthread_mtx, PTHREAD_PRIO_PROTECT);
            break;
        }
        case OSAL_MTX_NONE:
        {
            ret = pthread_mutexattr_setprotocol(&pthread_mtx, PTHREAD_PRIO_NONE);
            break;
        }
        default:
        {
            ret = pthread_mutexattr_setprotocol(&pthread_mtx, PTHREAD_PRIO_NONE);
            break;
        }
    }

    if (ret == SYS_SUCCESS)
        pthread_mutex_init(mutex, &pthread_mtx);

    pthread_mutexattr_destroy(&pthread_mtx);
}

sys_result_e osal_mutex_lock(osal_mutex_t *mutex)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = pthread_mutex_lock(mutex);
    return CHECK_ERROR(ret);
}

sys_result_e osal_mutex_unlock(osal_mutex_t *mutex)
{
    int ret = pthread_mutex_unlock(mutex);
    return CHECK_ERROR(ret);
}

sys_result_e osal_sem_init(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = 0;

    ret = sem_init(sem, 0, 0);

    return CHECK_ERROR(ret);
}

sys_result_e osal_sem_signal(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = sem_post(sem);

    return CHECK_ERROR(ret);
}

sys_result_e osal_sem_wait(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = sem_wait(sem);

    return CHECK_ERROR(ret);
}

void osal_task_wait_start(osal_id_t id)
{
    int sval, ret;

    ret = sem_getvalue(&osal_task_signals[id], &sval);
    if (sval < 0)
    {
        osal_sem_wait(&osal_task_signals[id]);
    }
}