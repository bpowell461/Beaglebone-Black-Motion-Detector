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
    INT32 signal;
    sigset_t signal_set;
    timer_t timer;
    BOOL_T istimerCreated;
}osal_sequencer_t;

static BOOL_T os_initialized = DEF_FALSE;

static osal_task_tcb_t osal_task_tcb[MAX_TASKS];
static osal_sem_t osal_task_signals[MAX_TASKS];
static osal_sequencer_t task_sequencers[MAX_TASKS];

static UINT08 task_idx = 0;

static osal_mutex_t os_task_mutex;
static osal_mutex_t os_timer_mutex;

static sigset_t osal_sig;

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

    sigemptyset(&osal_sig);
    for (UINT32 i = SIGRTMIN; i <= SIGRTMAX; i++)
        sigaddset(&osal_sig, i);
    sigprocmask(SIG_BLOCK, &osal_sig, NULL);

    for (UINT32 i = 0; i < MAX_TASKS; i++)
    {
        task_sequencers[i].istimerCreated = DEF_FALSE;
    }

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

    osal_sem_init(&osal_task_signals[task_idx]);

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

    int ret = osal_sem_signal(&osal_task_signals[id]);

    return CHECK_ERROR(ret);
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

    pthread_exit(NULL);

    osal_mutex_unlock(&os_task_mutex);
    /* END CRITICAL SECTION */
    
    return SYS_SUCCESS;
}

sys_result_e osal_task_suspend(osal_id_t id)
{
    if (!os_initialized)
        return SYS_FAILURE;

    int ret = osal_sem_wait(&osal_task_signals[id]);

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
    int sval;

    sem_getvalue(&osal_task_signals[id], &sval);
    if (sval < 0)
    {
        osal_sem_wait(&osal_task_signals[id]);
    }
}

void osal_task_delay(osal_id_t id)
{
    int sig;
    sigwait(&(task_sequencers[id].signal_set), &sig);
}

void osal_task_set_period(osal_id_t id, UINT32 period_ms)
{
    static int next_sig;
    int ret;
    unsigned int ns;
    unsigned int sec;
    struct sigevent sigev;
    struct itimerspec itval;

    if (!task_sequencers[id].istimerCreated)
    {
        /* Initialise next_sig first time through. We can't use static
           initialisation because SIGRTMIN is a function call, not a constant */
        if (next_sig == 0)
            next_sig = SIGRTMIN;

        /* Check that we have not run out of signals */
        if (next_sig > SIGRTMAX)
            exit(1);

        osal_mutex_lock(&os_timer_mutex);
        task_sequencers[id].signal = next_sig;
        next_sig++;

        /* Create the signal mask that will be used in wait_period */
        sigemptyset(&(task_sequencers[id].signal_set));
        sigaddset(&(task_sequencers[id].signal_set), task_sequencers[id].signal);

        /* Create a timer that will generate the signal we have chosen */
        sigev.sigev_notify = SIGEV_SIGNAL;
        sigev.sigev_signo = task_sequencers[id].signal;
        sigev.sigev_value.sival_ptr = (void *)&task_sequencers[id].timer;
        ret = timer_create(CLOCK_MONOTONIC, &sigev, &task_sequencers[id].timer);

        task_sequencers[id].istimerCreated = DEF_TRUE;

        osal_mutex_unlock(&os_timer_mutex);

        if (ret == -1)
            exit(1);
    }
    
    /* Make the timer periodic */
    sec = period_ms / MSEC_PER_SEC;
    ns = (period_ms - (sec * MSEC_PER_SEC)) * USEC_PER_MSEC * NSEC_PER_USEC;
    itval.it_interval.tv_sec = sec;
    itval.it_interval.tv_nsec = ns;
    itval.it_value.tv_sec = sec;
    itval.it_value.tv_nsec = ns;
    ret = timer_settime(task_sequencers[id].timer, 0, &itval, NULL);
}

void osal_task_wait_all(void)
{
    if (os_initialized)
    {
        for (UINT32 i = 0; i < MAX_TASKS; i++)
        {
            if (osal_task_tcb[i].task_func != DEF_NULL_PTR)
                pthread_join(osal_task_tcb[i].task_handle, NULL);
        } 
    }
}