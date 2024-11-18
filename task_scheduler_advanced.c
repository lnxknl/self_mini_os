#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "task_scheduler_advanced.h"

// Bandwidth Control Implementation
int init_bandwidth_control(struct bandwidth_control *bw,
                         unsigned long long period,
                         unsigned long long quota) {
    if (!bw || period < GROUP_MIN_PERIOD || period > GROUP_MAX_PERIOD)
        return -EINVAL;

    if (quota > period || quota < (period * MIN_BANDWIDTH_PERCENT) / 100)
        return -EINVAL;

    spin_lock_init(&bw->lock);
    bw->period = period;
    bw->quota = quota;
    bw->runtime = 0;
    bw->deadline = ktime_get_ns() + period;
    bw->flags = 0;
    memset(&bw->stats, 0, sizeof(struct group_stats));

    return 0;
}

void update_bandwidth_usage(struct bandwidth_control *bw,
                          unsigned long long runtime) {
    unsigned long long now = ktime_get_ns();
    unsigned long flags;

    spin_lock_irqsave(&bw->lock, flags);

    // Check if we need to start a new period
    if (now >= bw->deadline) {
        bw->runtime = 0;
        bw->deadline = now + bw->period;
        bw->flags &= ~GROUP_THROTTLED;
    }

    bw->runtime += runtime;
    bw->stats.total_runtime += runtime;

    // Check if we've exceeded quota
    if (bw->runtime > bw->quota && !(bw->flags & GROUP_THROTTLED)) {
        bw->flags |= GROUP_THROTTLED;
        bw->stats.nr_throttled++;
    }

    spin_unlock_irqrestore(&bw->lock, flags);
}

bool check_bandwidth_limit(struct bandwidth_control *bw) {
    unsigned long long now = ktime_get_ns();
    bool has_bandwidth;
    unsigned long flags;

    spin_lock_irqsave(&bw->lock, flags);

    // Reset period if needed
    if (now >= bw->deadline) {
        bw->runtime = 0;
        bw->deadline = now + bw->period;
        bw->flags &= ~GROUP_THROTTLED;
    }

    has_bandwidth = (bw->runtime < bw->quota);
    
    spin_unlock_irqrestore(&bw->lock, flags);
    return has_bandwidth;
}

// Group Management Implementation
struct task_group_advanced *create_task_group(struct task_group_advanced *parent) {
    struct task_group_advanced *group;
    int depth = parent ? parent->depth + 1 : 0;

    if (depth >= MAX_GROUP_DEPTH)
        return ERR_PTR(-EINVAL);

    group = kzalloc(sizeof(*group), GFP_KERNEL);
    if (!group)
        return ERR_PTR(-ENOMEM);

    // Initialize bandwidth control
    if (init_bandwidth_control(&group->bw, GROUP_MAX_PERIOD,
                             (GROUP_MAX_PERIOD * DEFAULT_BANDWIDTH) / 100)) {
        kfree(group);
        return ERR_PTR(-EINVAL);
    }

    group->depth = depth;
    group->parent = parent;
    INIT_LIST_HEAD(&group->children);
    INIT_LIST_HEAD(&group->siblings);
    INIT_LIST_HEAD(&group->tasks);
    spin_lock_init(&group->group_lock);
    
    if (parent) {
        unsigned long flags;
        spin_lock_irqsave(&parent->group_lock, flags);
        list_add_tail(&group->siblings, &parent->children);
        spin_unlock_irqrestore(&parent->group_lock, flags);
    }

    return group;
}

int attach_task_to_group(struct task_extension *task,
                        struct task_group_advanced *group) {
    unsigned long flags;

    if (!task || !group)
        return -EINVAL;

    spin_lock_irqsave(&group->group_lock, flags);
    
    // Remove from old group if any
    if (task->group) {
        spin_lock(&task->group->group_lock);
        list_del(&task->group_node);
        spin_unlock(&task->group->group_lock);
    }

    task->group = group;
    list_add_tail(&task->group_node, &group->tasks);
    
    spin_unlock_irqrestore(&group->group_lock, flags);
    return 0;
}

// Load Balancing Implementation
void init_cpu_domain(struct cpu_domain *domain, int cpu_id) {
    domain->cpu_id = cpu_id;
    domain->capacity = MAX_CPU_CAPACITY;
    domain->load = 0;
    domain->nr_running = 0;
    INIT_LIST_HEAD(&domain->groups);
    spin_lock_init(&domain->lock);
    domain->lb_state = LB_IDLE;
    domain->last_balance = 0;
    INIT_LIST_HEAD(&domain->siblings);
    INIT_LIST_HEAD(&domain->children);
}

int check_load_imbalance(struct cpu_domain *domain) {
    unsigned long avg_load = 0;
    unsigned long total_load = 0;
    int nr_cpus = 0;
    struct cpu_domain *sibling;

    // Calculate average load across siblings
    list_for_each_entry(sibling, &domain->siblings, siblings) {
        total_load += sibling->load;
        nr_cpus++;
    }

    if (nr_cpus == 0)
        return 0;

    avg_load = total_load / nr_cpus;
    
    // Check if this domain's load is significantly higher
    return (domain->load * 100 / avg_load) > LOAD_IMBALANCE_PCT;
}

struct task_extension *select_task_for_migration(struct cpu_domain *src,
                                               struct cpu_domain *dst) {
    struct task_extension *best_task = NULL;
    unsigned long min_interval = ULONG_MAX;
    struct task_group_advanced *group;
    struct task_extension *task;
    unsigned long flags;

    spin_lock_irqsave(&src->lock, flags);

    // Iterate through all groups in the source domain
    list_for_each_entry(group, &src->groups, siblings) {
        spin_lock(&group->group_lock);
        
        // Find a suitable task from this group
        list_for_each_entry(task, &group->tasks, group_node) {
            unsigned long interval;
            
            // Skip if task was recently migrated
            if (ktime_get_ns() - task->last_migration < MIGRATION_COST)
                continue;

            interval = task->slice_end - ktime_get_ns();
            if (interval < min_interval) {
                min_interval = interval;
                best_task = task;
            }
        }
        
        spin_unlock(&group->group_lock);
    }

    if (best_task) {
        // Update migration timestamp
        best_task->last_migration = ktime_get_ns();
        best_task->cpu_preferred = dst->cpu_id;
    }

    spin_unlock_irqrestore(&src->lock, flags);
    return best_task;
}

// Preemption Control Implementation
bool should_preempt_task(struct task_extension *curr,
                        struct task_extension *next) {
    unsigned long long now = ktime_get_ns();

    // Don't preempt if current task just started
    if (now - curr->exec_start < PREEMPTION_GRANULARITY)
        return false;

    // Check bandwidth limits
    if (!check_bandwidth_limit(&curr->group->bw) &&
        check_bandwidth_limit(&next->group->bw))
        return true;

    // Check vruntime differences
    if (next->vruntime + FAIR_LATENCY < curr->vruntime)
        return true;

    // Check if current task exceeded its slice
    if (now >= curr->slice_end)
        return true;

    return false;
}

void update_preemption_state(struct task_extension *task) {
    unsigned long long now = ktime_get_ns();
    unsigned long long slice;

    // Calculate time slice based on task's group share
    slice = (task->group->bw.quota * task->group->shares) /
            (task->group->load_avg + 1);

    // Ensure minimum granularity
    if (slice < PREEMPTION_GRANULARITY)
        slice = PREEMPTION_GRANULARITY;

    task->exec_start = now;
    task->slice_end = now + slice;
}

unsigned long calculate_preemption_delay(struct task_extension *curr,
                                       struct task_extension *next) {
    unsigned long long now = ktime_get_ns();
    unsigned long delay = 0;

    // If current task is near completion, let it finish
    if (curr->slice_end - now < PREEMPTION_GRANULARITY)
        delay = curr->slice_end - now;

    // Consider migration costs
    if (curr->cpu_preferred != next->cpu_preferred)
        delay = max(delay, MIGRATION_COST);

    return delay;
}
