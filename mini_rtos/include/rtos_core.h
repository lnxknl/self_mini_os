#ifndef RTOS_CORE_H
#define RTOS_CORE_H

#include "rtos_types.h"

/* System Initialization */
void rtos_init(void);
void rtos_start(void);

/* Task Management */
tcb_t *task_create(const char *name, 
                   task_function_t func, 
                   void *arg, 
                   uint8_t priority,
                   uint32_t stack_size);
void task_delete(tcb_t *task);
void task_suspend(tcb_t *task);
void task_resume(tcb_t *task);
void task_yield(void);
tcb_t *task_get_current(void);

/* Scheduling */
void scheduler_init(void);
void scheduler_start(void);
void scheduler_switch_task(void);
void scheduler_add_task(tcb_t *task);
void scheduler_remove_task(tcb_t *task);

/* Memory Management */
void *rtos_malloc(uint32_t size);
void rtos_free(void *ptr);
void memory_init(void);

/* Synchronization */
#if USE_MUTEX
mutex_t *mutex_create(void);
void mutex_delete(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
#endif

#if USE_SEMAPHORE
semaphore_t *semaphore_create(int32_t initial_count);
void semaphore_delete(semaphore_t *sem);
void semaphore_take(semaphore_t *sem);
void semaphore_give(semaphore_t *sem);
#endif

#if USE_QUEUE
queue_t *queue_create(uint32_t item_size, uint32_t item_count);
void queue_delete(queue_t *queue);
int queue_send(queue_t *queue, const void *item, uint32_t timeout);
int queue_receive(queue_t *queue, void *buffer, uint32_t timeout);
#endif

/* Time Management */
void delay_ms(uint32_t ms);
uint32_t get_tick_count(void);

/* System Protection */
void enter_critical(void);
void exit_critical(void);

/* Interrupt Handling */
void register_interrupt(uint32_t irq_num, isr_function_t handler);
void unregister_interrupt(uint32_t irq_num);

/* Debug and Statistics */
#if USE_STATS
void get_system_stats(system_stats_t *stats);
void get_task_stats(tcb_t *task, uint32_t *cpu_usage, uint32_t *stack_usage);
#endif

/* Stack Operations */
void stack_init(tcb_t *task);
int stack_check(tcb_t *task);

/* Context Switching */
void context_switch(void);
void systick_handler(void);
void pendsv_handler(void);

#endif /* RTOS_CORE_H */
