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
#include "syslog.h"

#define MAX_TASKS 4

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

typedef enum
{
    TASKSTATE_UNUSED,
    TASKSTATE_RUNNING,
    TASKSTATE_SUSPENDED,
    TASKSTATE_COUNT
}osal_task_state_e;

typedef struct
{
    UINT32 period_ms;
    UINT32 period_cnts;
    osal_sem_t signal_sem;
    osal_task_state_e task_state;
}osal_sequencer_t;

static BOOL_T os_initialized = DEF_FALSE;
static BOOL_T scheduler_started = DEF_FALSE;
static BOOL_T sequencerCallbackAssigned = DEF_FALSE;

static osal_task_tcb_t osal_task_tcb[MAX_TASKS];
static osal_sequencer_t osal_task_sequencers[MAX_TASKS];
static osal_task_tcb_t osal_scheduler_tcb;

static timer_t osal_task_timer;
static struct itimerspec itval;
static struct itimerspec oitval;

static UINT08 task_idx = 0;

static osal_mutex_t os_tcb_mutex;
static osal_mutex_t os_sched_mutex;
static osal_sem_t   os_sched_sem;

static int rt_max_prio;
static int rt_min_prio;
static pid_t mainpid;

static void osal_signal_handler_dispatcher(const INT32 signal);
static void *osal_signal_handler_task(void *threadp);
static void osal_task_generic_sequencer(void);
static sys_result_e osal_create_scheduler(void);

static unsigned long long tick = 0;

sys_result_e osal_init(void)
{
    if (os_initialized)
        return SYS_SUCCESS;

    struct sched_param main_param;
    int rc;

    mainpid = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO) - 2;
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio + 2;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);

    if (IS_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        osal_task_sequencers[i].task_state = TASKSTATE_UNUSED;
    }

    rc = timer_create(CLOCK_MONOTONIC, NULL, &osal_task_timer);
    if (IS_ERROR(rc))
    {
        return SYS_FAILURE;
    }

    os_initialized = DEF_TRUE;

    if (SYS_SUCCESS != osal_mutex_init(&os_tcb_mutex, OSAL_MTX_PRIO_CEILING))
    {
        return SYS_FAILURE;
    }

    if (SYS_SUCCESS != osal_mutex_init(&os_sched_mutex, OSAL_MTX_PRIO_CEILING))
    {
        return SYS_FAILURE;
    }

    if (SYS_SUCCESS != osal_sem_init(&os_sched_sem))
    {
        return SYS_FAILURE;
    }

    osal_create_scheduler();

    return SYS_SUCCESS;
}

sys_result_e osal_deinit(void)
{
    if (!os_initialized)
        return SYS_SUCCESS;

    for (osal_id_t i = 0; i < MAX_TASKS; i++)
    {
        osal_task_delete(i, DEF_TRUE);
    }

    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = 0;

    signal(SIGALRM, SIG_DFL);

    timer_settime(osal_task_timer, 0, &itval, &oitval);

    timer_delete(osal_task_timer);

    osal_mutex_delete(&os_tcb_mutex);

    osal_mutex_delete(&os_sched_mutex);

    os_initialized = DEF_FALSE;

    return SYS_SUCCESS;
}

sys_result_e osal_task_create(osal_id_t *id, char* name, osal_stack_t stack, osal_priority_t priority, osal_func task_func, UINT32 period_ms, void* args)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int res;
    pthread_attr_t attr;
    cpu_set_t threadcpu;

    if (priority > rt_max_prio || priority < rt_min_prio)
    {
        SYS_TRACE("ERR: TASK PRIORITIES");
        return SYS_FAILURE;
    }

    res = pthread_attr_init(&attr);
    if (IS_ERROR(res))
    {
        SYS_TRACE("ERR: CREATE TASK ATTR");
        return SYS_FAILURE;
    }

    struct sched_param sp;
    res = pthread_attr_getschedparam(&attr, &sp);

    CPU_ZERO(&threadcpu);
    CPU_SET(0, &threadcpu);

    res = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    res = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    res = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &threadcpu);

    sp.sched_priority = priority;
    pthread_attr_setschedparam(&attr, &sp);

    /* CRITICAL SECTION */
    osal_mutex_lock(&os_tcb_mutex);

    *id = task_idx;
    osal_task_tcb[task_idx].task_id = *id;
    osal_task_tcb[task_idx].task_name = name;
    osal_task_tcb[task_idx].task_stack = stack;
    osal_task_tcb[task_idx].task_priority = priority;
    osal_task_tcb[task_idx].task_func = task_func;
    osal_task_tcb[task_idx].task_args = (osal_task_start_args_t){ task_idx, args};

    res = osal_sem_init(&osal_task_sequencers[task_idx].signal_sem);
    if (IS_ERROR(res))
    {
        SYS_TRACE("ERR: TASK SEM INIT");
        goto cleanup;
    }

    res = pthread_create(&osal_task_tcb[task_idx].task_handle, &attr, task_func, (void *)&osal_task_tcb[task_idx].task_args);
    if (IS_ERROR(res))
    {
        SYS_TRACE("ERR: CREATE TASK");
        goto cleanup;
    }

    osal_task_set_period(task_idx, period_ms);

    if (scheduler_started)
    {
        osal_task_start(task_idx);
    }
    else
    {
        osal_task_sequencers[task_idx].task_state = TASKSTATE_SUSPENDED;
    }
    
    task_idx++;

cleanup: 
    osal_mutex_unlock(&os_tcb_mutex);
    /* END CRITICAL SECTION */

    pthread_attr_destroy(&attr);

    return SYS_SUCCESS;
}

sys_result_e osal_task_start(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    osal_mutex_lock(&os_sched_mutex);
    if (SYS_SUCCESS != osal_sem_signal(&osal_task_sequencers[id].signal_sem))
    {
        osal_mutex_unlock(&os_sched_mutex);
        SYS_TRACE("ERR: TASK START (%s)", osal_task_tcb[id].task_name);
        return SYS_FAILURE;
    }

    osal_task_sequencers[task_idx].task_state = TASKSTATE_RUNNING;
    osal_mutex_unlock(&os_sched_mutex);
    return SYS_SUCCESS;
}

sys_result_e osal_task_delete(osal_id_t id, BOOL_T force)
{
    if (!os_initialized)
        return SYS_FAILURE;

    osal_mutex_lock(&os_sched_mutex);
    /* CRITICAL SECTION */
   osal_task_tcb[id].task_name = "";
   osal_task_tcb[id].task_stack = 0;
   osal_task_tcb[id].task_priority = 0;
   osal_task_tcb[id].task_func = DEF_NULL_PTR;
   osal_task_tcb[id].task_args = (osal_task_start_args_t){ 0, 0 };

    osal_sem_delete(&osal_task_sequencers[id].signal_sem);
    osal_task_sequencers[id].task_state = TASKSTATE_UNUSED;
    /* END CRITICAL SECTION */
    osal_mutex_unlock(&os_sched_mutex);

    if (force)
        pthread_cancel(osal_task_tcb[id].task_handle);
    else
        pthread_exit(DEF_NULL_PTR);

    return SYS_SUCCESS;
}

sys_result_e osal_task_suspend(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    osal_mutex_lock(&os_sched_mutex);
    osal_task_sequencers[id].task_state = TASKSTATE_SUSPENDED;
    osal_mutex_unlock(&os_sched_mutex);

    return osal_sem_wait(&osal_task_sequencers[id].signal_sem);
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
    return IS_ERROR(pthread_mutex_destroy(mutex));
}

sys_result_e osal_mutex_lock(osal_mutex_t *mutex)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = pthread_mutex_lock(mutex);
    return IS_ERROR(ret);
}

sys_result_e osal_mutex_unlock(osal_mutex_t *mutex)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = pthread_mutex_unlock(mutex);
    return IS_ERROR(ret);
}

sys_result_e osal_sem_init(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = 0;

    ret = sem_init(sem, 0, 0);

    return IS_ERROR(ret);
}

sys_result_e osal_sem_delete(osal_sem_t *sem)
{
    return IS_ERROR(sem_destroy(sem));
}

sys_result_e osal_sem_signal(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = sem_post(sem);

    return IS_ERROR(ret);
}

sys_result_e osal_sem_wait(osal_sem_t *sem)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = sem_wait(sem);

    return IS_ERROR(ret);
}

void osal_task_wait_start(osal_id_t id)
{
    int sval;

    sem_getvalue(&osal_task_sequencers[id].signal_sem, &sval);
    if (sval < 0)
    {
        osal_sem_wait(&osal_task_sequencers[id].signal_sem);
    }
}

void osal_task_delay(osal_id_t id)
{
    osal_task_suspend(id);
}

void osal_task_delay_period(osal_id_t id, UINT32 period_ms)
{
    osal_task_set_period(id, period_ms);
    osal_task_suspend(id);
}

void osal_task_set_period(osal_id_t id, UINT32 period_ms)
{
    osal_mutex_lock(&os_sched_mutex);
    /* REQ: Period must be a multiple of timer tick and >= the timer tick resolution */
    osal_task_sequencers[id].period_ms = period_ms;
    osal_task_sequencers[id].period_cnts = (period_ms / TIMER_TICK_MS);
    osal_mutex_unlock(&os_sched_mutex);
}

UINT32 osal_task_wait_all(void)
{
    UINT32 runningTasks = 0;
    if (os_initialized)
    {
        for (UINT32 i = 0; i < MAX_TASKS; i++)
        {
            if (TASKSTATE_UNUSED != osal_task_sequencers[i].task_state)
            {
                pthread_join(osal_task_tcb[i].task_handle, NULL);
                runningTasks++;
            }    
        } 
    }

    return runningTasks;
}

sys_result_e osal_task_wait(osal_id_t id)
{
    if (os_initialized)
    {
        if (TASKSTATE_UNUSED != osal_task_sequencers[id].task_state)
        {
            pthread_join(osal_task_tcb[id].task_handle, NULL);
            return SYS_SUCCESS;
        }
    }

    return SYS_FAILURE;
}

sys_result_e osal_start_scheduler(void)
{
    if (!os_initialized)
        return SYS_FAILURE;

    INT32 ret;
    struct sigaction action;

    osal_mutex_lock(&os_sched_mutex);

    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = (TIMER_TICK_MS * USEC_PER_MSEC * NSEC_PER_USEC);
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = (TIMER_TICK_MS * USEC_PER_MSEC * NSEC_PER_USEC);

    if (!sequencerCallbackAssigned)
    {
        action.sa_handler = osal_signal_handler_dispatcher;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        sigaction(SIGALRM, &action, NULL);

        sequencerCallbackAssigned = DEF_TRUE;
    }

    ret = timer_settime(osal_task_timer, 0, &itval, &oitval);

    scheduler_started = DEF_TRUE;

    osal_mutex_unlock(&os_sched_mutex);

    return IS_ERROR(ret);
}
sys_result_e osal_stop_scheduler(void)
{
    if (!os_initialized)
        return SYS_SUCCESS;

    INT32 ret;

    osal_mutex_lock(&os_sched_mutex);

    itval.it_interval.tv_sec = 0;
    itval.it_interval.tv_nsec = 0;
    itval.it_value.tv_sec = 0;
    itval.it_value.tv_nsec = 0;

    ret = timer_settime(osal_task_timer, 0, &itval, &oitval);

    scheduler_started = DEF_FALSE;

    osal_mutex_unlock(&os_sched_mutex);

    return IS_ERROR(ret);
}

static sys_result_e osal_create_scheduler(void)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int res;
    pthread_attr_t attr;
    cpu_set_t threadcpu;

    res = pthread_attr_init(&attr);
    if (IS_ERROR(res))
    {
        SYS_TRACE("ERR: CREATE TASK ATTR");
        return SYS_FAILURE;
    }

    struct sched_param sp;
    res = pthread_attr_getschedparam(&attr, &sp);

    CPU_ZERO(&threadcpu);
    CPU_SET(0, &threadcpu);

    res = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    res = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    res = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &threadcpu);

    sp.sched_priority = rt_max_prio + 1;
    pthread_attr_setschedparam(&attr, &sp);

    /* CRITICAL SECTION */
    osal_mutex_lock(&os_tcb_mutex);

    osal_scheduler_tcb.task_id = MAX_TASKS;
    osal_scheduler_tcb.task_name = "Scheduler";
    osal_scheduler_tcb.task_stack = 0;
    osal_scheduler_tcb.task_priority = (osal_priority_t)(rt_max_prio + 1);
    osal_scheduler_tcb.task_func = osal_signal_handler_task;
    osal_scheduler_tcb.task_args = (osal_task_start_args_t){ MAX_TASKS, NULL };

    res = pthread_create(&osal_scheduler_tcb.task_handle, &attr, osal_signal_handler_task, (void *)&osal_scheduler_tcb.task_args);
    if (IS_ERROR(res))
    {
        SYS_TRACE("ERR: CREATE TASK");
        goto cleanup;
    }

cleanup:
    osal_mutex_unlock(&os_tcb_mutex);
    /* END CRITICAL SECTION */

    pthread_attr_destroy(&attr);

    return SYS_SUCCESS;
}

static void osal_signal_handler_dispatcher(const INT32 signal)
{
    osal_sem_signal(&os_sched_sem);
    tick++;
}

static void* osal_signal_handler_task(void* threadp)
{
    while (DEF_TRUE)
    {
        osal_sem_wait(&os_sched_sem);
        osal_task_generic_sequencer();
    }

    pthread_exit(NULL);
}

static inline void osal_task_generic_sequencer(void)
{
    osal_mutex_lock(&os_sched_mutex);
    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        if (osal_task_sequencers[i].task_state != TASKSTATE_UNUSED && osal_task_sequencers[i].period_cnts != 0)
        {
            if ((tick % osal_task_sequencers[i].period_cnts) == 0)
            {
                osal_task_start((osal_id_t)i);
            }
        }
    }
    osal_mutex_unlock(&os_sched_mutex);
}