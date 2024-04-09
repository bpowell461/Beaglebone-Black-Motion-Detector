/**
 * @file osal.h
 * @author Brad Powell
 * @date 18 Mar 2024
 * @brief File containing definitions for Operating System Abstraction Layer functions.
 *
 */

#ifndef OSAL_H_
#define OSAL_H_

#include "osal_cfg.h"

sys_result_e osal_init(void);

sys_result_e osal_task_create(osal_id_t *id, char *name, osal_stack_t stack, osal_priority_t priority, osal_func task_func, void *args);

sys_result_e osal_task_start(osal_id_t id);

sys_result_e osal_task_delete(osal_id_t id);

sys_result_e osal_task_suspend(osal_id_t id);

sys_result_e osal_mutex_init(osal_mutex_t *mutex, osal_mutex_attr_e attr);

sys_result_e osal_mutex_lock(osal_mutex_t *mutex);

sys_result_e osal_mutex_unlock(osal_mutex_t *mutex);

sys_result_e osal_sem_init(osal_sem_t *sem);

sys_result_e osal_sem_signal(osal_sem_t *sem);

sys_result_e osal_sem_wait(osal_sem_t *sem); 

void osal_task_wait_start(osal_id_t id);

#endif // OSAL_H_