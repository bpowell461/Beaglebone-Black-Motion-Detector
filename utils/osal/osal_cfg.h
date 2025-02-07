/**
 * @file osal.h
 * @author Brad Powell
 * @date 18 Mar 2024
 * @brief File containing definitions for Operating System Abstraction Layer functions.
 *
 */

#ifndef OSAL_CFG_H_
#define OSAL_CFG_H_

/* Hook your OS here */
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include "types.h"
#include "ringbuffer.h"

/* OS Specific Hooks */
typedef pthread_mutex_t     osal_mutex_t;
typedef sem_t               osal_sem_t;
typedef pthread_t           osal_task_t;
typedef mqd_t               osal_mqueue_t;

/* Configuration Types */
typedef uint8_t osal_id_t;
typedef uint16_t osal_stack_t;

typedef void *(*osal_func)(void *);

typedef uint8_t osal_priority_t;

typedef struct
{
    osal_id_t task_id;
    void *args;
}osal_task_start_args_t;

typedef enum
{
    OSAL_MTX_PRIO_INHERIT,
    OSAL_MTX_PRIO_CEILING,
    OSAL_MTX_NONE,
    OSAL_MTX_COUNT
}osal_mutex_attr_e;

#endif /* OSAL_CFG_H_ */
