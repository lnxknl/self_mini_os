#ifndef _TASK_SCHEDULER_ADVANCED_H
#define _TASK_SCHEDULER_ADVANCED_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/list.h>

// Advanced scheduling parameters
#define GROUP_MIN_PERIOD     1000000     // 1ms in ns
#define GROUP_MAX_PERIOD     1000000000  // 1s in ns
#define MIN_BANDWIDTH_PERCENT 1          // 1% minimum bandwidth
#define MAX_BANDWIDTH_PERCENT 100        // 100% maximum bandwidth
#define DEFAULT_BANDWIDTH    70          // 70% default bandwidth
#define LOAD_BALANCE_PERIOD  4000000     // 4ms load balance interval
#define MIGRATION_COST       1000000     // 1ms migration cost
#define PREEMPTION_GRANULARITY 100000    // 100us granularity
#define MAX_GROUP_DEPTH      8           // Maximum group nesting depth
#define LOAD_IMBALANCE_PCT  125         // 25% load imbalance threshold

// Load balancing states
enum load_balance_state {
    LB_IDLE = 0,
    LB_ACTIVE,
    LB_FAILED,
    LB_THROTTLED
};

// Group scheduling flags
#define GROUP_THROTTLED      (1 << 0)
#define GROUP_REDISTRIBUTED  (1 << 1)
#define GROUP_ISOLATED      (1 << 2)
#define GROUP_OVERCOMMIT    (1 << 3)

// Task group statistics
struct group_stats {
    unsigned long long total_runtime;
    unsigned long long throttled_time;
    unsigned long nr_throttled;
    unsigned long nr_migrations;
    unsigned long nr_preemptions;
    unsigned long load_avg;
    unsigned long cpu_time[NR_CPUS];
};

// Bandwidth control structure
struct bandwidth_control {
    unsigned long long period;
    unsigned long long quota;
    unsigned long long runtime;
    unsigned long long deadline;
    spinlock_t lock;
    unsigned int flags;
    struct group_stats stats;
};

// CPU domain structure for hierarchical load balancing
struct cpu_domain {
    int cpu_id;
    unsigned long capacity;
    unsigned long load;
    unsigned long nr_running;
    struct list_head groups;
    struct bandwidth_control bw;
    spinlock_t lock;
    enum load_balance_state lb_state;
    unsigned long last_balance;
    struct cpu_domain *parent;
    struct list_head siblings;
    struct list_head children;
};

// Enhanced task group structure
struct task_group_advanced {
    struct task_group *linux_tg;    // Link to standard Linux task group
    struct bandwidth_control bw;
    struct group_stats stats;
    int depth;                      // Nesting depth
    unsigned int flags;
    struct task_group_advanced *parent;
    struct list_head children;
    struct list_head siblings;
    struct list_head tasks;
    struct cpu_domain *preferred_domain;
    spinlock_t group_lock;
    unsigned long shares;           // CPU shares
    unsigned long load_avg;         // Exponential weighted moving average
    unsigned long long last_update;
};

// Task extension for advanced features
struct task_extension {
    struct task_struct *task;
    struct task_group_advanced *group;
    unsigned long long vruntime;
    unsigned long long exec_start;
    unsigned long long slice_end;
    unsigned long load_avg;
    int cpu_preferred;
    unsigned int flags;
    struct list_head group_node;
    struct rb_node load_node;
    unsigned long long last_migration;
};

// Function declarations for bandwidth control
int init_bandwidth_control(struct bandwidth_control *bw, 
                         unsigned long long period,
                         unsigned long long quota);
void update_bandwidth_usage(struct bandwidth_control *bw,
                          unsigned long long runtime);
bool check_bandwidth_limit(struct bandwidth_control *bw);
void reset_bandwidth_period(struct bandwidth_control *bw);

// Function declarations for group management
struct task_group_advanced *create_task_group(struct task_group_advanced *parent);
int attach_task_to_group(struct task_extension *task,
                        struct task_group_advanced *group);
void update_group_statistics(struct task_group_advanced *group,
                           struct task_extension *task,
                           unsigned long long runtime);

// Function declarations for load balancing
void init_cpu_domain(struct cpu_domain *domain, int cpu_id);
int check_load_imbalance(struct cpu_domain *domain);
struct cpu_domain *select_busiest_domain(struct cpu_domain *root);
struct cpu_domain *select_idlest_domain(struct cpu_domain *root);
struct task_extension *select_task_for_migration(struct cpu_domain *src,
                                               struct cpu_domain *dst);

// Function declarations for preemption control
bool should_preempt_task(struct task_extension *curr,
                        struct task_extension *next);
void update_preemption_state(struct task_extension *task);
unsigned long calculate_preemption_delay(struct task_extension *curr,
                                       struct task_extension *next);

#endif /* _TASK_SCHEDULER_ADVANCED_H */
