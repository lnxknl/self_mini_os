#ifndef SCHEDULER_RT_H
#define SCHEDULER_RT_H

#include "rtos_types.h"
#include <stdint.h>

/* Real-Time Task Parameters */
typedef struct {
    uint32_t period;          /* Task period in ticks */
    uint32_t deadline;        /* Relative deadline */
    uint32_t execution_time;  /* Worst-case execution time */
    uint32_t release_time;    /* Next release time */
    uint8_t is_periodic;      /* Periodic or aperiodic task */
} rt_params_t;

/* Task Priority Management */
typedef enum {
    PRIORITY_POLICY_FIXED = 0,
    PRIORITY_POLICY_DYNAMIC_RM,  /* Rate Monotonic */
    PRIORITY_POLICY_DYNAMIC_EDF, /* Earliest Deadline First */
    PRIORITY_POLICY_DYNAMIC_LLF  /* Least Laxity First */
} priority_policy_t;

/* Real-Time Task Control Block Extension */
typedef struct {
    tcb_t *task;
    rt_params_t params;
    uint32_t actual_execution_time;
    uint32_t deadline_misses;
    uint32_t completion_time;
    float cpu_utilization;
} rt_tcb_t;

/* Real-Time Scheduler Configuration */
typedef struct {
    priority_policy_t policy;
    uint8_t allow_priority_inheritance;
    uint8_t use_slack_stealing;
    uint8_t monitor_deadlines;
    uint32_t monitoring_period;
} rt_scheduler_config_t;

/* Scheduling Analysis Results */
typedef struct {
    uint8_t is_schedulable;
    float total_utilization;
    uint32_t hyperperiod;
    uint32_t min_deadline;
    uint32_t max_response_time;
} schedule_analysis_t;

/* Real-Time Scheduler Functions */
void rt_scheduler_init(rt_scheduler_config_t *config);
int rt_task_create(const char *name, task_function_t func, void *arg,
                  rt_params_t *params, rt_tcb_t **task);
void rt_task_delete(rt_tcb_t *task);

/* Priority Management */
void set_priority_policy(priority_policy_t policy);
void update_task_priority(rt_tcb_t *task);
void priority_inheritance_enable(void);
void priority_inheritance_disable(void);

/* Deadline Management */
int check_deadline(rt_tcb_t *task);
void handle_deadline_miss(rt_tcb_t *task);
uint32_t get_next_deadline(rt_tcb_t *task);

/* Scheduling Analysis */
schedule_analysis_t *analyze_schedule(rt_tcb_t **tasks, uint32_t task_count);
uint8_t is_schedule_feasible(rt_tcb_t **tasks, uint32_t task_count);
float calculate_cpu_utilization(rt_tcb_t **tasks, uint32_t task_count);

/* Execution Time Monitoring */
void start_execution_monitoring(rt_tcb_t *task);
void stop_execution_monitoring(rt_tcb_t *task);
uint32_t get_execution_time(rt_tcb_t *task);

/* Slack Stealing */
uint32_t calculate_available_slack(void);
void update_slack_time(rt_tcb_t *task);

/* Statistics and Monitoring */
typedef struct {
    uint32_t total_jobs_executed;
    uint32_t deadline_misses;
    uint32_t max_response_time;
    uint32_t min_response_time;
    float avg_response_time;
    float cpu_utilization;
    uint32_t context_switches;
    uint32_t priority_inversions;
} rt_task_stats_t;

void get_rt_task_stats(rt_tcb_t *task, rt_task_stats_t *stats);
void reset_rt_task_stats(rt_tcb_t *task);

#endif /* SCHEDULER_RT_H */
