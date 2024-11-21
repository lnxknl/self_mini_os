#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include <stdint.h>
#include <stdbool.h>
#include "memory_order.h"

/* Load balancing policies */
typedef enum {
    LB_POLICY_ROUND_ROBIN,      // Simple round-robin distribution
    LB_POLICY_LEAST_LOADED,     // Assign to least loaded core
    LB_POLICY_PRIORITY_AWARE,   // Consider task priorities
    LB_POLICY_MEMORY_AFFINITY,  // Optimize for memory access patterns
    LB_POLICY_THERMAL_AWARE,    // Consider core temperatures
    LB_POLICY_HYBRID           // Combination of multiple policies
} lb_policy_t;

/* Core load metrics */
typedef struct {
    atomic_uint32_t cpu_usage;          // CPU utilization percentage
    atomic_uint32_t task_count;         // Number of active tasks
    atomic_uint32_t memory_pressure;    // Memory pressure indicator
    atomic_uint32_t cache_misses;       // Cache miss rate
    atomic_uint32_t temperature;        // Core temperature
    atomic_uint32_t power_consumption;  // Power usage metric
} core_metrics_t;

/* Task requirements */
typedef struct {
    uint32_t priority;           // Task priority level
    uint32_t cpu_requirement;    // Expected CPU usage
    uint32_t memory_requirement; // Expected memory usage
    uint32_t deadline;          // Deadline in ticks
    bool realtime;              // Real-time requirements
    uint32_t affinity_mask;     // Core affinity mask
} task_requirements_t;

/* Load balancer statistics */
typedef struct {
    atomic_uint64_t total_migrations;     // Total task migrations
    atomic_uint64_t balanced_tasks;       // Successfully balanced tasks
    atomic_uint64_t failed_balancing;     // Failed balancing attempts
    atomic_uint64_t policy_switches;      // Number of policy switches
    atomic_uint64_t thermal_throttling;   // Thermal throttling events
    atomic_uint64_t deadline_misses;      // Missed deadlines
} lb_stats_t;

/* Load balancer configuration */
typedef struct {
    lb_policy_t policy;
    uint32_t update_interval;    // Metrics update interval
    uint32_t migration_threshold;// Load imbalance threshold
    uint32_t max_migrations;     // Maximum migrations per interval
    bool thermal_throttling;     // Enable thermal throttling
    bool dynamic_policy;         // Enable dynamic policy switching
} lb_config_t;

/* Initialize the load balancer */
bool lb_init(lb_config_t *config);

/* Update core metrics */
void lb_update_metrics(uint32_t core_id, core_metrics_t *metrics);

/* Balance a new task */
uint32_t lb_balance_task(task_requirements_t *task);

/* Migrate an existing task */
bool lb_migrate_task(uint32_t task_id, uint32_t src_core, uint32_t dst_core);

/* Get current load balancing statistics */
void lb_get_stats(lb_stats_t *stats);

/* Dynamic policy adjustment */
void lb_adjust_policy(lb_policy_t new_policy);

/* Check system balance */
bool lb_check_balance(void);

/* Thermal management */
void lb_handle_thermal_event(uint32_t core_id, uint32_t temperature);

/* Memory pressure handling */
void lb_handle_memory_pressure(uint32_t core_id, uint32_t pressure);

/* Asynchronous operations */
bool lb_migrate_task_async(uint32_t task_id, uint32_t src_core, uint32_t dst_core);
void lb_update_metrics_async(uint32_t core_id, core_metrics_t* metrics);
void lb_adjust_policy_async(lb_policy_t new_policy, bool force_update);
void lb_handle_thermal_event_async(uint32_t core_id, uint32_t temperature, bool emergency);
void lb_handle_memory_pressure_async(uint32_t core_id, uint32_t pressure, bool force_rebalance);

#endif /* LOAD_BALANCER_H */
