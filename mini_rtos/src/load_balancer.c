#include "load_balancer.h"
#include "memory_order.h"
#include <string.h>
#include "async_ops.h"

#define MAX_CORES 32
#define THERMAL_THRESHOLD 85
#define MEMORY_PRESSURE_THRESHOLD 90
#define LOAD_IMBALANCE_THRESHOLD 20

/* Global load balancer state */
static struct {
    atomic_bool initialized;
    lb_config_t config;
    core_metrics_t core_metrics[MAX_CORES];
    lb_stats_t stats;
    atomic_uint32_t active_cores;
    atomic_uint32_t total_load;
    void *lock;
} lb_state;

/* Initialize the load balancer */
bool lb_init(lb_config_t *config) {
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit_bool(
            &lb_state.initialized, &expected, true,
            MEMORY_ORDER_ACQ_REL, MEMORY_ORDER_RELAXED)) {
        return false;
    }

    memcpy(&lb_state.config, config, sizeof(lb_config_t));
    memset(&lb_state.stats, 0, sizeof(lb_stats_t));
    
    atomic_store_explicit_u32(&lb_state.active_cores, 0, MEMORY_ORDER_RELEASE);
    atomic_store_explicit_u32(&lb_state.total_load, 0, MEMORY_ORDER_RELEASE);
    
    lb_state.lock = mutex_create();
    
    return true;
}

/* Calculate core load score based on multiple metrics */
static uint32_t calculate_core_load(core_metrics_t *metrics) {
    uint32_t cpu_score = atomic_load_explicit_u32(&metrics->cpu_usage, MEMORY_ORDER_ACQUIRE);
    uint32_t mem_score = atomic_load_explicit_u32(&metrics->memory_pressure, MEMORY_ORDER_ACQUIRE);
    uint32_t temp_score = atomic_load_explicit_u32(&metrics->temperature, MEMORY_ORDER_ACQUIRE);
    uint32_t tasks = atomic_load_explicit_u32(&metrics->task_count, MEMORY_ORDER_ACQUIRE);
    
    return (cpu_score * 4 + mem_score * 2 + temp_score + tasks * 2) / 9;
}

/* Find least loaded core considering task requirements */
static uint32_t find_best_core(task_requirements_t *task) {
    uint32_t best_core = 0;
    uint32_t min_load = UINT32_MAX;
    uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);

    for (uint32_t i = 0; i < active_cores; i++) {
        if (!(task->affinity_mask & (1 << i))) {
            continue;
        }

        core_metrics_t *metrics = &lb_state.core_metrics[i];
        uint32_t core_load = calculate_core_load(metrics);
        
        // Check thermal constraints
        if (atomic_load_explicit_u32(&metrics->temperature, MEMORY_ORDER_ACQUIRE) >= THERMAL_THRESHOLD) {
            continue;
        }

        // Check memory pressure
        if (atomic_load_explicit_u32(&metrics->memory_pressure, MEMORY_ORDER_ACQUIRE) >= MEMORY_PRESSURE_THRESHOLD) {
            continue;
        }

        // Consider task priority
        if (task->priority > 0) {
            core_load = core_load * (10 - task->priority) / 10;
        }

        if (core_load < min_load) {
            min_load = core_load;
            best_core = i;
        }
    }

    return best_core;
}

/* Balance a new task */
uint32_t lb_balance_task(task_requirements_t *task) {
    mutex_lock(lb_state.lock);
    
    uint32_t target_core;
    switch (lb_state.config.policy) {
        case LB_POLICY_ROUND_ROBIN: {
            static atomic_uint32_t next_core = ATOMIC_VAR_INIT(0);
            uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
            target_core = atomic_fetch_add_explicit_u32(&next_core, 1, MEMORY_ORDER_ACQ_REL) % active_cores;
            break;
        }
        
        case LB_POLICY_LEAST_LOADED:
            target_core = find_best_core(task);
            break;
            
        case LB_POLICY_PRIORITY_AWARE:
            if (task->priority >= 8) {
                // High priority tasks get dedicated cores if possible
                target_core = find_best_core(task);
            } else {
                // Lower priority tasks are balanced for throughput
                target_core = find_best_core(task);
            }
            break;
            
        case LB_POLICY_MEMORY_AFFINITY: {
            // Find core with best memory access patterns
            uint32_t best_core = 0;
            uint32_t min_misses = UINT32_MAX;
            uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
            
            for (uint32_t i = 0; i < active_cores; i++) {
                uint32_t misses = atomic_load_explicit_u32(&lb_state.core_metrics[i].cache_misses, MEMORY_ORDER_ACQUIRE);
                if (misses < min_misses) {
                    min_misses = misses;
                    best_core = i;
                }
            }
            target_core = best_core;
            break;
        }
            
        case LB_POLICY_THERMAL_AWARE: {
            // Find coolest core that meets requirements
            uint32_t best_core = 0;
            uint32_t min_temp = UINT32_MAX;
            uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
            
            for (uint32_t i = 0; i < active_cores; i++) {
                uint32_t temp = atomic_load_explicit_u32(&lb_state.core_metrics[i].temperature, MEMORY_ORDER_ACQUIRE);
                if (temp < min_temp && temp < THERMAL_THRESHOLD) {
                    min_temp = temp;
                    best_core = i;
                }
            }
            target_core = best_core;
            break;
        }
            
        case LB_POLICY_HYBRID:
            // Combine multiple factors
            target_core = find_best_core(task);
            break;
            
        default:
            target_core = 0;
            break;
    }

    // Update core metrics
    atomic_fetch_add_explicit_u32(&lb_state.core_metrics[target_core].task_count, 1, MEMORY_ORDER_ACQ_REL);
    atomic_fetch_add_explicit_u32(&lb_state.core_metrics[target_core].cpu_usage, task->cpu_requirement, MEMORY_ORDER_ACQ_REL);
    
    // Update statistics
    atomic_fetch_add_explicit_u64(&lb_state.stats.balanced_tasks, 1, MEMORY_ORDER_RELAXED);
    
    mutex_unlock(lb_state.lock);
    return target_core;
}

/* Migrate an existing task */
bool lb_migrate_task(uint32_t task_id, uint32_t src_core, uint32_t dst_core) {
    if (src_core == dst_core) {
        return false;
    }

    mutex_lock(lb_state.lock);
    
    // Check migration constraints
    if (atomic_load_explicit_u32(&lb_state.core_metrics[dst_core].temperature, MEMORY_ORDER_ACQUIRE) >= THERMAL_THRESHOLD) {
        atomic_fetch_add_explicit_u64(&lb_state.stats.failed_balancing, 1, MEMORY_ORDER_RELAXED);
        mutex_unlock(lb_state.lock);
        return false;
    }

    // Perform migration
    atomic_fetch_sub_explicit_u32(&lb_state.core_metrics[src_core].task_count, 1, MEMORY_ORDER_ACQ_REL);
    atomic_fetch_add_explicit_u32(&lb_state.core_metrics[dst_core].task_count, 1, MEMORY_ORDER_ACQ_REL);
    
    // Update statistics
    atomic_fetch_add_explicit_u64(&lb_state.stats.total_migrations, 1, MEMORY_ORDER_RELAXED);
    
    mutex_unlock(lb_state.lock);
    return true;
}

/* Update core metrics */
void lb_update_metrics(uint32_t core_id, core_metrics_t *metrics) {
    memcpy(&lb_state.core_metrics[core_id], metrics, sizeof(core_metrics_t));
    
    // Check for thermal throttling
    if (metrics->temperature >= THERMAL_THRESHOLD && lb_state.config.thermal_throttling) {
        atomic_fetch_add_explicit_u64(&lb_state.stats.thermal_throttling, 1, MEMORY_ORDER_RELAXED);
        lb_handle_thermal_event(core_id, metrics->temperature);
    }
    
    // Check memory pressure
    if (metrics->memory_pressure >= MEMORY_PRESSURE_THRESHOLD) {
        lb_handle_memory_pressure(core_id, metrics->memory_pressure);
    }
}

/* Get current load balancing statistics */
void lb_get_stats(lb_stats_t *stats) {
    memcpy(stats, &lb_state.stats, sizeof(lb_stats_t));
}

/* Dynamic policy adjustment */
void lb_adjust_policy(lb_policy_t new_policy) {
    if (!lb_state.config.dynamic_policy) {
        return;
    }
    
    mutex_lock(lb_state.lock);
    lb_state.config.policy = new_policy;
    atomic_fetch_add_explicit_u64(&lb_state.stats.policy_switches, 1, MEMORY_ORDER_RELAXED);
    mutex_unlock(lb_state.lock);
}

/* Check system balance */
bool lb_check_balance(void) {
    uint32_t max_load = 0;
    uint32_t min_load = UINT32_MAX;
    uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
    
    for (uint32_t i = 0; i < active_cores; i++) {
        uint32_t load = calculate_core_load(&lb_state.core_metrics[i]);
        max_load = (load > max_load) ? load : max_load;
        min_load = (load < min_load) ? load : min_load;
    }
    
    return (max_load - min_load) <= LOAD_IMBALANCE_THRESHOLD;
}

/* Thermal management */
void lb_handle_thermal_event(uint32_t core_id, uint32_t temperature) {
    if (!lb_state.config.thermal_throttling) {
        return;
    }
    
    mutex_lock(lb_state.lock);
    
    // Migrate tasks from hot core
    uint32_t task_count = atomic_load_explicit_u32(&lb_state.core_metrics[core_id].task_count, MEMORY_ORDER_ACQUIRE);
    uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
    
    for (uint32_t i = 0; i < active_cores; i++) {
        if (i == core_id) continue;
        
        if (atomic_load_explicit_u32(&lb_state.core_metrics[i].temperature, MEMORY_ORDER_ACQUIRE) < THERMAL_THRESHOLD) {
            // Migrate half of tasks to cooler core
            uint32_t tasks_to_migrate = task_count / 2;
            atomic_fetch_sub_explicit_u32(&lb_state.core_metrics[core_id].task_count, tasks_to_migrate, MEMORY_ORDER_ACQ_REL);
            atomic_fetch_add_explicit_u32(&lb_state.core_metrics[i].task_count, tasks_to_migrate, MEMORY_ORDER_ACQ_REL);
            break;
        }
    }
    
    mutex_unlock(lb_state.lock);
}

/* Memory pressure handling */
void lb_handle_memory_pressure(uint32_t core_id, uint32_t pressure) {
    mutex_lock(lb_state.lock);
    
    // Find core with lowest memory pressure
    uint32_t best_core = core_id;
    uint32_t min_pressure = pressure;
    uint32_t active_cores = atomic_load_explicit_u32(&lb_state.active_cores, MEMORY_ORDER_ACQUIRE);
    
    for (uint32_t i = 0; i < active_cores; i++) {
        if (i == core_id) continue;
        
        uint32_t core_pressure = atomic_load_explicit_u32(&lb_state.core_metrics[i].memory_pressure, MEMORY_ORDER_ACQUIRE);
        if (core_pressure < min_pressure) {
            min_pressure = core_pressure;
            best_core = i;
        }
    }
    
    if (best_core != core_id) {
        // Migrate some tasks to reduce memory pressure
        uint32_t task_count = atomic_load_explicit_u32(&lb_state.core_metrics[core_id].task_count, MEMORY_ORDER_ACQUIRE);
        uint32_t tasks_to_migrate = task_count / 4;  // Migrate 25% of tasks
        
        atomic_fetch_sub_explicit_u32(&lb_state.core_metrics[core_id].task_count, tasks_to_migrate, MEMORY_ORDER_ACQ_REL);
        atomic_fetch_add_explicit_u32(&lb_state.core_metrics[best_core].task_count, tasks_to_migrate, MEMORY_ORDER_ACQ_REL);
    }
    
    mutex_unlock(lb_state.lock);
}

/* Async operation parameters */
typedef struct {
    uint32_t task_id;
    uint32_t src_core;
    uint32_t dst_core;
} migration_params_t;

typedef struct {
    uint32_t core_id;
    core_metrics_t metrics;
} load_update_params_t;

typedef struct {
    lb_policy_t new_policy;
    bool force_update;
} policy_change_params_t;

typedef struct {
    uint32_t core_id;
    uint32_t temperature;
    bool emergency;
} thermal_params_t;

typedef struct {
    uint32_t core_id;
    uint32_t pressure;
    bool force_rebalance;
} memory_params_t;

/* Async callbacks */
static void migration_complete(void* context, async_status_t status, void* result) {
    if (status == ASYNC_STATUS_COMPLETED) {
        atomic_fetch_add_explicit_u64(&lb_state.stats.total_migrations, 1, MEMORY_ORDER_RELAXED);
    }
}

static void thermal_complete(void* context, async_status_t status, void* result) {
    if (status == ASYNC_STATUS_COMPLETED) {
        atomic_fetch_add_explicit_u64(&lb_state.stats.thermal_throttling, 1, MEMORY_ORDER_RELAXED);
    }
}

static void memory_complete(void* context, async_status_t status, void* result) {
    if (status == ASYNC_STATUS_COMPLETED) {
        atomic_fetch_add_explicit_u64(&lb_state.stats.memory_rebalances, 1, MEMORY_ORDER_RELAXED);
    }
}

/* Async operation handlers */
static bool handle_task_migration(void* params) {
    migration_params_t* mp = (migration_params_t*)params;
    return lb_migrate_task(mp->task_id, mp->src_core, mp->dst_core);
}

static bool handle_load_update(void* params) {
    load_update_params_t* lp = (load_update_params_t*)params;
    lb_update_metrics(lp->core_id, &lp->metrics);
    return true;
}

static bool handle_policy_change(void* params) {
    policy_change_params_t* pp = (policy_change_params_t*)params;
    lb_adjust_policy(pp->new_policy);
    return true;
}

static bool handle_thermal_adjust(void* params) {
    thermal_params_t* tp = (thermal_params_t*)params;
    lb_handle_thermal_event(tp->core_id, tp->temperature);
    return true;
}

static bool handle_memory_rebalance(void* params) {
    memory_params_t* mp = (memory_params_t*)params;
    lb_handle_memory_pressure(mp->core_id, mp->pressure);
    return true;
}

/* Async public interfaces */
bool lb_migrate_task_async(uint32_t task_id, uint32_t src_core, uint32_t dst_core) {
    migration_params_t params = {
        .task_id = task_id,
        .src_core = src_core,
        .dst_core = dst_core
    };
    
    uint32_t op_id = async_submit(ASYNC_OP_TASK_MIGRATION, &params, sizeof(params),
                                 migration_complete, NULL, get_system_time_ms() + 1000, 8);
    return op_id != 0;
}

void lb_update_metrics_async(uint32_t core_id, core_metrics_t* metrics) {
    load_update_params_t params = {
        .core_id = core_id
    };
    memcpy(&params.metrics, metrics, sizeof(core_metrics_t));
    
    async_submit(ASYNC_OP_LOAD_UPDATE, &params, sizeof(params),
                NULL, NULL, get_system_time_ms() + 100, 5);
}

void lb_adjust_policy_async(lb_policy_t new_policy, bool force_update) {
    policy_change_params_t params = {
        .new_policy = new_policy,
        .force_update = force_update
    };
    
    async_submit(ASYNC_OP_POLICY_CHANGE, &params, sizeof(params),
                NULL, NULL, get_system_time_ms() + 500, 6);
}

void lb_handle_thermal_event_async(uint32_t core_id, uint32_t temperature, bool emergency) {
    thermal_params_t params = {
        .core_id = core_id,
        .temperature = temperature,
        .emergency = emergency
    };
    
    uint32_t priority = emergency ? 9 : 7;
    async_submit(ASYNC_OP_THERMAL_ADJUST, &params, sizeof(params),
                thermal_complete, NULL, get_system_time_ms() + 200, priority);
}

void lb_handle_memory_pressure_async(uint32_t core_id, uint32_t pressure, bool force_rebalance) {
    memory_params_t params = {
        .core_id = core_id,
        .pressure = pressure,
        .force_rebalance = force_rebalance
    };
    
    uint32_t priority = force_rebalance ? 8 : 6;
    async_submit(ASYNC_OP_MEMORY_REBALANCE, &params, sizeof(params),
                memory_complete, NULL, get_system_time_ms() + 300, priority);
}
