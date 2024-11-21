#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "rtos_types.h"

/* Scheduling Policies */
typedef enum {
    SCHED_PRIORITY = 0,  /* Fixed Priority Preemptive */
    SCHED_RR,           /* Round Robin */
    SCHED_EDF,          /* Earliest Deadline First */
    SCHED_HYBRID        /* Priority + RR for same priority */
} sched_policy_t;

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

/* Preemption Control */
#define PREEMPT_DISABLE     0
#define PREEMPT_ENABLE      1
#define PREEMPT_COND        2  /* Conditional preemption */

/* Task Flags */
#define TASK_FLAG_NONE          0x00
#define TASK_FLAG_PREEMPTABLE   0x01  /* Task can be preempted */
#define TASK_FLAG_CRITICAL      0x02  /* Task is in critical section */
#define TASK_FLAG_INHERITED     0x04  /* Task has inherited priority */
#define TASK_FLAG_DEADLINE      0x08  /* Task has deadline */

/* Task Control Block */
typedef struct tcb {
    /* Task Identification */
    uint32_t id;
    char name[32];
    
    /* Task State */
    task_state_t state;
    uint8_t priority;
    uint8_t original_priority;  /* For priority inheritance */
    uint32_t time_slice;
    uint32_t ticks_remaining;
    uint32_t flags;
    
    /* Scheduling Info */
    sched_policy_t policy;
    uint32_t deadline;
    uint32_t period;
    uint32_t execution_time;
    uint32_t arrival_time;
    
    /* Stack Management */
    void *stack_ptr;
    void *stack_base;
    uint32_t stack_size;
    
    /* Context Information */
    uint32_t *context;
    void (*entry_point)(void *);
    void *arg;
    
    /* Priority Inheritance */
    struct tcb *blocked_by;    /* Task blocking this task */
    struct tcb *blocking;      /* Tasks being blocked by this task */
    uint32_t inheritance_depth;
    
    /* Statistics */
    uint32_t run_time;
    uint32_t last_run;
    uint32_t num_activations;
    uint32_t num_preemptions;
    uint32_t deadline_misses;
    
    /* List Management */
    struct tcb *next;
    struct tcb *prev;
} tcb_t;

/* Task Creation and Management */
tcb_t *task_create(const char *name, void (*entry)(void *), void *arg,
                   uint8_t priority, uint32_t stack_size);
tcb_t *task_create_extended(const char *name, void (*entry)(void *), void *arg,
                           uint8_t priority, uint32_t stack_size, 
                           sched_policy_t policy, uint32_t deadline);
void task_delete(tcb_t *task);
void task_suspend(tcb_t *task);
void task_resume(tcb_t *task);

/* Priority Management */
void task_set_priority(tcb_t *task, uint8_t new_priority);
void task_boost_priority(tcb_t *task, uint8_t boost_priority);
void task_restore_priority(tcb_t *task);

/* Scheduling Control */
void scheduler_init(void);
void scheduler_start(void);
void schedule_next_task(void);
tcb_t *get_current_task(void);
void set_scheduling_policy(sched_policy_t policy);

/* Preemption Control */
void set_preemption_mode(uint8_t mode);
uint8_t get_preemption_mode(void);
void disable_preemption(void);
void enable_preemption(void);

/* Task Synchronization */
void task_yield(void);
void task_delay(uint32_t ticks);
void task_sleep_until(uint32_t wake_tick);
void task_set_deadline(tcb_t *task, uint32_t deadline);

/* Priority Inheritance */
void inherit_priority(tcb_t *blocker, tcb_t *blocked);
void release_inherited_priority(tcb_t *task);

/* EDF Scheduling */
void edf_schedule(void);
int check_deadline_miss(tcb_t *task);
void update_task_deadlines(void);

/* Round Robin */
void rr_schedule(void);
void set_time_slice(tcb_t *task, uint32_t slice);

/* Task Statistics */
typedef struct {
    uint32_t total_tasks;
    uint32_t running_tasks;
    uint32_t blocked_tasks;
    uint32_t deadline_misses;
    uint32_t priority_inversions;
    uint32_t preemptions;
    uint32_t context_switches;
    uint32_t cpu_usage;
} scheduler_stats_t;

void get_scheduler_stats(scheduler_stats_t *stats);
void dump_task_info(tcb_t *task);
void list_all_tasks(void);

#endif /* SCHEDULER_H */
