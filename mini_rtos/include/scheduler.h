#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "rtos_types.h"

/* Task States */
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_TERMINATED
} task_state_t;

/* Task Priority Levels */
#define TASK_PRIORITY_IDLE      0
#define TASK_PRIORITY_LOW       1
#define TASK_PRIORITY_NORMAL    2
#define TASK_PRIORITY_HIGH      3
#define TASK_PRIORITY_REALTIME  4
#define MAX_PRIORITY_LEVELS     5

/* Task Configuration */
#define MAX_TASKS           32
#define TASK_STACK_SIZE     1024
#define IDLE_TASK_STACK_SIZE 512

/* Task Control Block */
typedef struct tcb {
    /* Task Identification */
    uint32_t id;
    char name[32];
    
    /* Task State */
    task_state_t state;
    uint8_t priority;
    uint32_t time_slice;
    uint32_t ticks_remaining;
    
    /* Stack Management */
    void *stack_ptr;              /* Current stack pointer */
    void *stack_base;             /* Base of stack */
    uint32_t stack_size;
    
    /* Context Information */
    uint32_t *context;            /* Saved context (registers) */
    void (*entry_point)(void *);  /* Task entry function */
    void *arg;                    /* Task argument */
    
    /* Statistics */
    uint32_t run_time;
    uint32_t last_run;
    uint32_t num_activations;
    
    /* Scheduling */
    uint32_t deadline;
    uint32_t period;
    
    /* List Management */
    struct tcb *next;
    struct tcb *prev;
} tcb_t;

/* Task Creation and Management */
tcb_t *task_create(const char *name, void (*entry)(void *), void *arg,
                   uint8_t priority, uint32_t stack_size);
void task_delete(tcb_t *task);
void task_suspend(tcb_t *task);
void task_resume(tcb_t *task);
void task_set_priority(tcb_t *task, uint8_t new_priority);

/* Scheduler Control */
void scheduler_init(void);
void scheduler_start(void);
void schedule_next_task(void);
tcb_t *get_current_task(void);

/* Task Synchronization */
void task_yield(void);
void task_delay(uint32_t ticks);
void task_sleep_until(uint32_t wake_tick);

/* Task Statistics */
typedef struct {
    uint32_t total_tasks;
    uint32_t running_tasks;
    uint32_t blocked_tasks;
    uint32_t cpu_usage;
    uint32_t context_switches;
    uint32_t task_switches;
} scheduler_stats_t;

void get_scheduler_stats(scheduler_stats_t *stats);
void dump_task_info(tcb_t *task);
void list_all_tasks(void);

#endif /* SCHEDULER_H */
