/**
 * @file osal.c
 * @author Brad Powell
 * @date 18 Mar 2024
 * @brief File containing definitions for Operating System Abstraction Layer functions.
 *
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include "osal.h"
#include "utils.h"

#define MAX_TASKS 128
#define CHECK_ERROR(x)  ((x) ? SYS_FAILURE : SYS_SUCCESS)

#define TIMER_TICK_MS (10U)

typedef struct
{
    osal_id_t               task_id;
    char*                   task_name;
    osal_stack_t            task_stack;
    osal_priority_t         task_priority;
    osal_func               task_func;
    osal_task_start_args_t  task_args;
    osal_task_t             task_handle;
}osal_task_tcb_t;

typedef struct
{
    UINT32 period_ms;
    UINT32 period_cnts;
    osal_sem_t signal_sem;
    BOOL_T isTaskRunning;
}osal_sequencer_t;

static BOOL_T os_initialized = DEF_FALSE;

static osal_task_tcb_t osal_task_tcb[MAX_TASKS];
static osal_sequencer_t osal_task_sequencers[MAX_TASKS];

static timer_t osal_task_timer;
static struct itimerspec itval;
static struct itimerspec oitval;

static UINT08 task_idx = 0;

static osal_mutex_t os_task_mutex;
static osal_mutex_t os_timer_mutex;

static int rt_max_prio;
static int rt_min_prio;
static pid_t mainpid;

static void osal_task_generic_sequencer(INT32 id);

sys_result_e osal_init(void)
{
    if (os_initialized)
        return SYS_SUCCESS;

    struct sched_param main_param;
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

    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        osal_task_tcb[i].task_func = DEF_NULL_PTR;
        osal_task_sequencers[i].isTaskRunning = DEF_FALSE;
    }

    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = (TIMER_TICK_MS * USEC_PER_MSEC * NSEC_PER_SEC);
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = (TIMER_TICK_MS * USEC_PER_MSEC * NSEC_PER_SEC);

    rc = timer_create(CLOCK_MONOTONIC, NULL, &osal_task_timer);
    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    signal(SIGALRM, (void(*)()) osal_task_generic_sequencer);

    timer_settime(osal_task_timer, 0, &itval, &oitval);

    os_initialized = DEF_TRUE;

    rc = osal_mutex_init(&os_task_mutex, OSAL_MTX_PRIO_CEILING);
    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    rc = osal_mutex_init(&os_timer_mutex, OSAL_MTX_PRIO_CEILING);
    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    return SYS_SUCCESS;
}

sys_result_e osal_deinit(void)
{
    if (!os_initialized)
        return SYS_SUCCESS;

    int rc;

    if (CHECK_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        osal_task_delete(i);
    }

    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = 0;

    signal(SIGALRM, SIG_DFL);

    timer_settime(osal_task_timer, 0, &itval, &oitval);

    timer_delete(osal_task_timer);

    rc = osal_mutex_delete(&os_task_mutex);

    rc = osal_mutex_delete(&os_timer_mutex);

    os_initialized = DEF_FALSE;

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
    res = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &threadcpu);

    sp.sched_priority = priority.priority;
    pthread_attr_setschedparam(&attr, &sp);

    /* CRITICAL SECTION */
    osal_mutex_lock(&os_task_mutex);

    *id = task_idx;
    osal_task_tcb[task_idx].task_id = *id;
    osal_task_tcb[task_idx].task_name = name;
    osal_task_tcb[task_idx].task_stack = stack;
    osal_task_tcb[task_idx].task_priority = priority;
    osal_task_tcb[task_idx].task_func = task_func;
    osal_task_tcb[task_idx].task_args = (osal_task_start_args_t){ task_idx, args};

    osal_sem_init(&osal_task_sequencers[task_idx].signal_sem);

    res = pthread_create(&osal_task_tcb[task_idx].task_handle, &attr, task_func, (void *)&osal_task_tcb[task_idx].task_args);
    if (!CHECK_ERROR(res))
    {
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

    sys_result_e ret = osal_sem_signal(&osal_task_sequencers[task_idx].signal_sem);
    if (!CHECK_ERROR(ret))
    {
        osal_task_sequencers[task_idx].isTaskRunning = DEF_TRUE;
    }

    return SYS_SUCCESS;
}

sys_result_e osal_task_delete(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    /* CRITICAL SECTION */
    osal_mutex_lock(&os_task_mutex);

    osal_task_tcb[id].task_name = "";
    osal_task_tcb[id].task_stack = 0;
    osal_task_tcb[id].task_priority = (osal_priority_t){0, 0};
    osal_task_tcb[id].task_func = DEF_NULL_PTR;
    osal_task_tcb[id].task_args = (osal_task_start_args_t){0, 0};
    osal_task_sequencers[task_idx].isTaskRunning = DEF_FALSE;

    pthread_cancel(osal_task_tcb[id].task_handle);

    osal_mutex_unlock(&os_task_mutex);
    /* END CRITICAL SECTION */
    
    return SYS_SUCCESS;
}

sys_result_e osal_task_suspend(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    osal_task_sequencers[task_idx].isTaskRunning = DEF_FALSE;
    int ret = osal_sem_wait(&osal_task_sequencers[task_idx].signal_sem);

    return CHECK_ERROR(ret);
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

    return SYS_SUCCESS;
}

sys_result_e osal_mutex_delete(osal_mutex_t *mutex)
{
    return CHECK_ERROR(pthread_mutex_destroy(mutex));
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

sys_result_e osal_sem_delete(osal_sem_t *sem)
{
    return CHECK_ERROR(sem_destroy(sem));
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
    int sval;

    sem_getvalue(&osal_task_sequencers[task_idx].signal_sem, &sval);
    if (sval < 0)
    {
        osal_sem_wait(&osal_task_sequencers[task_idx].signal_sem);
    }
}

void osal_task_delay(osal_id_t id)
{
    osal_sem_wait(&osal_task_sequencers[id].signal_sem);
}

void osal_task_set_period(osal_id_t id, UINT32 period_ms)
{
    osal_mutex_lock(&os_timer_mutex);
    if (osal_task_sequencers[id].isTaskRunning)
    {
        /* REQ: Period must be a multiple of timer tick and >= the timer tick resolution */
        osal_task_sequencers[id].period_ms = period_ms;
        osal_task_sequencers[id].period_cnts = (period_ms / TIMER_TICK_MS);
    }
    osal_mutex_unlock(&os_timer_mutex);
}

void osal_task_wait_all(void)
{
    if (os_initialized)
    {
        /* We make the assumption that the 0th index is the bootstrap thread which we won't wait on */
        for (UINT32 i = 1; i < MAX_TASKS; i++)
        {
            if (osal_task_sequencers[i].isTaskRunning)
                pthread_join(osal_task_tcb[i].task_handle, NULL);
        } 
    }
}

void osal_task_wait_id(osal_id_t id)
{
    if (os_initialized)
    {
        if (osal_task_sequencers[id].isTaskRunning)
            pthread_join(osal_task_tcb[id].task_handle, NULL);
    }
}

static void osal_task_generic_sequencer(INT32 id)
{
    static unsigned long long cnt = 0;

    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        if (osal_task_sequencers[i].isTaskRunning)
        {
            if ((cnt % osal_task_sequencers[i].period_ms) == 0)
                osal_sem_signal(&osal_task_sequencers[i].signal_sem);
        }
    }
}