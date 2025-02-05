/**
 * @file osal.h
 * @author Brad Powell
 * @date 18 Mar 2024
 * @brief File containing definitions for Operating System Abstraction Layer functions.
 *
 */

#ifndef OSAL_H_
#define OSAL_H_

#include <unistd.h>
#include "osal_cfg.h"
#include "osal_tasks_cfg.h"
#include "types.h"

sys_result_e osal_init(void);

sys_result_e osal_deinit(void);

sys_result_e osal_task_create(osal_id_t *id, char *name, osal_stack_t stack, osal_priority_t priority, osal_func task_func, osal_func exit_func, uint32_t period_ms, void *args);

sys_result_e osal_task_start(osal_id_t id);

sys_result_e osal_task_delete(osal_id_t id, uint8_t force);

sys_result_e osal_task_suspend(osal_id_t id);

sys_result_e osal_mutex_init(osal_mutex_t *mutex, osal_mutex_attr_e attr);

sys_result_e osal_mutex_delete(osal_mutex_t *mutex);

sys_result_e osal_mutex_lock(osal_mutex_t *mutex);

sys_result_e osal_mutex_unlock(osal_mutex_t *mutex);

sys_result_e osal_sem_init(osal_sem_t *sem);

sys_result_e osal_sem_delete(osal_sem_t *sem);

sys_result_e osal_sem_signal(osal_sem_t *sem);

sys_result_e osal_sem_wait(osal_sem_t *sem); 

void osal_task_wait_start(osal_id_t id);

void osal_task_delay(osal_id_t id);

void osal_task_delay_period(osal_id_t id, uint32_t period_ms);

void osal_task_set_period(osal_id_t id, uint32_t period_ms);

uint32_t osal_task_wait_all(void);

sys_result_e osal_task_wait(osal_id_t id);

sys_result_e osal_start_scheduler(void);

sys_result_e osal_stop_scheduler(void);

#endif // OSAL_H_