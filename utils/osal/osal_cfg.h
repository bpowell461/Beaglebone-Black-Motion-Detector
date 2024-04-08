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
#include "types.h"
#include "ringbuffer.h"

/* OS Specific Hooks */
typedef pthread_mutex_t     osal_mutex_t;
typedef sem_t               osal_sem_t;
typedef pthread_t           osal_task_t;

/* Application Specific Hooks */
typedef buffer_t        osal_buffer_t;

/* Configuration Types */
typedef UINT08 osal_id_t;
typedef UINT16 osal_stack_t;

typedef void *(*osal_func)(void *);

typedef struct
{
    UINT08  priority;
    UINT08  subpriority;
}osal_priority_t;

typedef enum
{
    OSAL_MTX_PRIO_INHERIT,
    OSAL_MTX_PRIO_CEILING,
    OSAL_MTX_NONE,
    OSAL_MTX_COUNT
}osal_mutex_attr_e;

#endif /* OSAL_CFG_H_ */
