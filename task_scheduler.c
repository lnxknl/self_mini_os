#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom CFS Scheduler");
MODULE_DESCRIPTION("An advanced Linux CFS-like task scheduler implementation");

// Task and CPU states
#define TASK_RUNNING      0
#define TASK_READY        1
#define TASK_BLOCKED      2
#define TASK_TERMINATED   3

// Priority and weight definitions
#define MAX_PRIO 100
#define MIN_PRIO 0
#define DEFAULT_PRIO 50
#define NICE_TO_WEIGHT(nice) ((nice) + 20)
#define MIN_GRANULARITY 1000000    // 1ms in ns
#define FAIR_LATENCY    6000000    // 6ms in ns
#define MAX_CPU_CAPACITY 1024      // Maximum CPU capacity units
#define BANDWIDTH_PERIOD 100000000 // 100ms in ns
#define DEFAULT_GROUP_QUOTA 70     // 70% default CPU quota

// Forward declarations
struct task_group;
struct cpu_rq;

// Task structure
struct custom_task {
    pid_t pid;
    int state;
    int nice;
    unsigned int weight;
    unsigned long long vruntime;    // Virtual runtime in ns
    unsigned long long exec_start;   // Start of execution
    unsigned long long sum_exec_runtime; // Total runtime
    struct rb_node node;           // For red-black tree
    struct list_head list;         // For task lists
    struct task_group *group;      // Task group membership
    int cpu;                       // Current CPU
    unsigned long long deadline;   // Deadline for bandwidth control
    unsigned long long runtime_left; // Remaining runtime in current period
};

// Task group structure
struct task_group {
    struct rb_root tasks_tree;     // Tasks in this group
    unsigned int weight;           // Group weight
    unsigned long long vruntime;   // Group virtual runtime
    unsigned long long quota;      // CPU bandwidth quota (in ns)
    unsigned long long runtime;    // Runtime used in current period
    unsigned long long period;     // Bandwidth period
    struct task_group *parent;     // Parent group
    struct list_head children;     // Child groups
    struct list_head siblings;     // Sibling groups
    spinlock_t group_lock;        // Group lock
    int cpu_preferred;            // Preferred CPU for load balancing
};

// CPU runqueue structure
struct cpu_rq {
    struct rb_root tasks_timeline; // Red-black tree of tasks
    struct task_group *root_group; // Root task group
    unsigned long nr_running;      // Number of running tasks
    unsigned long cpu_load;        // CPU load
    spinlock_t lock;              // Runqueue lock
    int cpu;                      // CPU ID
    struct custom_task *curr;     // Currently running task
    unsigned long long min_vruntime; // Minimum vruntime
    unsigned long capacity;        // CPU capacity
};

// Global variables
static struct cpu_rq *cpu_rqs;    // Array of CPU runqueues
static int nr_cpus;               // Number of CPUs
static DEFINE_SPINLOCK(balancing_lock);  // Load balancing lock

// Initialize CPU runqueues
static void init_cpu_rqs(void) {
    int i;
    nr_cpus = num_online_cpus();
    cpu_rqs = kmalloc(sizeof(struct cpu_rq) * nr_cpus, GFP_KERNEL);

    for (i = 0; i < nr_cpus; i++) {
        cpu_rqs[i].tasks_timeline = RB_ROOT;
        cpu_rqs[i].nr_running = 0;
        cpu_rqs[i].cpu_load = 0;
        cpu_rqs[i].cpu = i;
        cpu_rqs[i].curr = NULL;
        cpu_rqs[i].min_vruntime = 0;
        cpu_rqs[i].capacity = MAX_CPU_CAPACITY;
        spin_lock_init(&cpu_rqs[i].lock);
    }
}

// Create a new task group
static struct task_group *create_task_group(struct task_group *parent) {
    struct task_group *group = kmalloc(sizeof(struct task_group), GFP_KERNEL);
    if (!group)
        return NULL;

    group->tasks_tree = RB_ROOT;
    group->weight = NICE_TO_WEIGHT(0);
    group->vruntime = 0;
    group->quota = (BANDWIDTH_PERIOD * DEFAULT_GROUP_QUOTA) / 100;
    group->period = BANDWIDTH_PERIOD;
    group->runtime = 0;
    group->parent = parent;
    INIT_LIST_HEAD(&group->children);
    INIT_LIST_HEAD(&group->siblings);
    spin_lock_init(&group->group_lock);
    group->cpu_preferred = -1;

    if (parent) {
        spin_lock(&parent->group_lock);
        list_add_tail(&group->siblings, &parent->children);
        spin_unlock(&parent->group_lock);
    }

    return group;
}

// Calculate CPU load
static unsigned long calc_cpu_load(struct cpu_rq *rq) {
    return rq->nr_running * NICE_TO_WEIGHT(0);
}

// Find busiest CPU for load balancing
static int find_busiest_cpu(void) {
    int cpu, busiest_cpu = 0;
    unsigned long max_load = 0;

    for (cpu = 0; cpu < nr_cpus; cpu++) {
        unsigned long load = calc_cpu_load(&cpu_rqs[cpu]);
        if (load > max_load) {
            max_load = load;
            busiest_cpu = cpu;
        }
    }
    return busiest_cpu;
}

// Find idlest CPU for load balancing
static int find_idlest_cpu(void) {
    int cpu, idlest_cpu = 0;
    unsigned long min_load = ULONG_MAX;

    for (cpu = 0; cpu < nr_cpus; cpu++) {
        unsigned long load = calc_cpu_load(&cpu_rqs[cpu]);
        if (load < min_load) {
            min_load = load;
            idlest_cpu = cpu;
        }
    }
    return idlest_cpu;
}

// Check if task's group has enough runtime quota
static bool check_group_runtime(struct custom_task *task) {
    struct task_group *group = task->group;
    unsigned long long now = ktime_get_ns();
    bool has_runtime = true;

    while (group) {
        spin_lock(&group->group_lock);
        if (now - group->runtime >= group->period) {
            // Reset period
            group->runtime = 0;
        }
        if (group->runtime >= group->quota) {
            has_runtime = false;
        }
        spin_unlock(&group->group_lock);
        
        if (!has_runtime)
            break;
        group = group->parent;
    }
    return has_runtime;
}

// Update group runtime
static void update_group_runtime(struct custom_task *task, unsigned long long delta) {
    struct task_group *group = task->group;

    while (group) {
        spin_lock(&group->group_lock);
        group->runtime += delta;
        group->vruntime += delta;
        spin_unlock(&group->group_lock);
        group = group->parent;
    }
}

// Load balance between CPUs
static void load_balance(void) {
    int busiest_cpu, idlest_cpu;
    struct cpu_rq *busiest, *idlest;
    struct custom_task *task;
    struct rb_node *node;

    spin_lock(&balancing_lock);
    
    busiest_cpu = find_busiest_cpu();
    idlest_cpu = find_idlest_cpu();
    
    if (busiest_cpu == idlest_cpu) {
        spin_unlock(&balancing_lock);
        return;
    }

    busiest = &cpu_rqs[busiest_cpu];
    idlest = &cpu_rqs[idlest_cpu];

    spin_lock(&busiest->lock);
    spin_lock(&idlest->lock);

    // Move task from busiest to idlest CPU
    node = rb_first(&busiest->tasks_timeline);
    if (node) {
        task = rb_entry(node, struct custom_task, node);
        rb_erase(&task->node, &busiest->tasks_timeline);
        busiest->nr_running--;
        task->cpu = idlest_cpu;
        rb_link_node(&task->node, NULL, &idlest->tasks_timeline.rb_node);
        rb_insert_color(&task->node, &idlest->tasks_timeline);
        idlest->nr_running++;
    }

    spin_unlock(&idlest->lock);
    spin_unlock(&busiest->lock);
    spin_unlock(&balancing_lock);
}

// Advanced preemption check
static bool should_preempt(struct custom_task *curr, struct custom_task *next) {
    if (!curr)
        return true;

    // Check group runtime quotas
    if (!check_group_runtime(curr) && check_group_runtime(next))
        return true;

    // Check vruntime differences
    if (next->vruntime + FAIR_LATENCY < curr->vruntime)
        return true;

    // Check deadlines for bandwidth control
    if (next->deadline && curr->deadline && next->deadline < curr->deadline)
        return true;

    return false;
}

// Update task's runtime and check for bandwidth control
static void update_curr_advanced(struct custom_task *task, struct cpu_rq *rq) {
    unsigned long long now = ktime_get_ns();
    unsigned long long delta_exec;

    if (!task->exec_start)
        return;

    delta_exec = now - task->exec_start;
    task->sum_exec_runtime += delta_exec;

    // Update group runtime
    update_group_runtime(task, delta_exec);

    // Update bandwidth control
    if (task->runtime_left <= delta_exec) {
        task->runtime_left = 0;
        task->deadline = now + BANDWIDTH_PERIOD;
    } else {
        task->runtime_left -= delta_exec;
    }

    // Update vruntime
    task->vruntime += (delta_exec * NICE_TO_WEIGHT(0)) / task->weight;
    if (task->vruntime < rq->min_vruntime)
        rq->min_vruntime = task->vruntime;
}

// Main scheduling function with all enhancements
static void schedule(void) {
    struct custom_task *prev, *next;
    struct cpu_rq *rq;
    int cpu = smp_processor_id();
    unsigned long long now;

    rq = &cpu_rqs[cpu];
    spin_lock(&rq->lock);

    prev = rq->curr;
    if (prev) {
        update_curr_advanced(prev, rq);
        if (prev->state == TASK_RUNNING) {
            prev->state = TASK_READY;
            rb_link_node(&prev->node, NULL, &rq->tasks_timeline.rb_node);
            rb_insert_color(&prev->node, &rq->tasks_timeline);
        }
    }

    // Select next task
    next = rb_entry(rb_first(&rq->tasks_timeline), struct custom_task, node);
    
    // Check if we should perform load balancing
    if (!next || rq->nr_running > (rq->capacity / NICE_TO_WEIGHT(0))) {
        spin_unlock(&rq->lock);
        load_balance();
        spin_lock(&rq->lock);
        next = rb_entry(rb_first(&rq->tasks_timeline), struct custom_task, node);
    }

    if (!next) {
        spin_unlock(&rq->lock);
        printk(KERN_INFO "No tasks to schedule on CPU %d\n", cpu);
        return;
    }

    // Check preemption
    if (!should_preempt(prev, next)) {
        spin_unlock(&rq->lock);
        return;
    }

    // Prepare next task
    rb_erase(&next->node, &rq->tasks_timeline);
    now = ktime_get_ns();
    next->exec_start = now;
    next->state = TASK_RUNNING;
    
    // Reset bandwidth control if period expired
    if (now >= next->deadline) {
        next->runtime_left = (next->group->quota * BANDWIDTH_PERIOD) / 100;
        next->deadline = now + BANDWIDTH_PERIOD;
    }

    rq->curr = next;
    rq->nr_running = rb_first(&rq->tasks_timeline) ? 1 : 0;

    spin_unlock(&rq->lock);

    printk(KERN_INFO "CPU %d scheduling task PID: %d, vruntime: %llu, group runtime left: %llu\n",
           cpu, next->pid, next->vruntime, next->runtime_left);
}

// Initialize a new task with group support
static struct custom_task *create_task(pid_t pid, int nice, struct task_group *group) {
    struct custom_task *task = kmalloc(sizeof(struct custom_task), GFP_KERNEL);
    if (!task)
        return NULL;

    task->pid = pid;
    task->state = TASK_READY;
    task->nice = nice;
    task->weight = calc_weight(nice);
    task->vruntime = 0;
    task->exec_start = 0;
    task->sum_exec_runtime = 0;
    task->group = group;
    task->cpu = -1;
    task->deadline = ktime_get_ns() + BANDWIDTH_PERIOD;
    task->runtime_left = (group->quota * BANDWIDTH_PERIOD) / 100;
    RB_CLEAR_NODE(&task->node);
    INIT_LIST_HEAD(&task->list);

    return task;
}

// API functions
int custom_create_task(pid_t pid, int nice, struct task_group *group) {
    struct custom_task *task;
    int cpu;

    if (nice < -20 || nice > 19)
        return -EINVAL;

    if (!group)
        return -EINVAL;

    task = create_task(pid, nice, group);
    if (!task)
        return -ENOMEM;

    // Assign to least loaded CPU
    cpu = find_idlest_cpu();
    task->cpu = cpu;

    spin_lock(&cpu_rqs[cpu].lock);
    rb_link_node(&task->node, NULL, &cpu_rqs[cpu].tasks_timeline.rb_node);
    rb_insert_color(&task->node, &cpu_rqs[cpu].tasks_timeline);
    cpu_rqs[cpu].nr_running++;
    spin_unlock(&cpu_rqs[cpu].lock);

    return 0;
}
EXPORT_SYMBOL(custom_create_task);

// Module initialization
static int __init custom_scheduler_init(void) {
    init_cpu_rqs();
    printk(KERN_INFO "Advanced CFS-like scheduler initialized with %d CPUs\n", nr_cpus);
    return 0;
}

// Module cleanup
static void __exit custom_scheduler_exit(void) {
    int cpu;
    struct rb_node *node;
    struct custom_task *task;

    // Clean up all CPU runqueues
    for (cpu = 0; cpu < nr_cpus; cpu++) {
        spin_lock(&cpu_rqs[cpu].lock);
        while ((node = rb_first(&cpu_rqs[cpu].tasks_timeline))) {
            task = rb_entry(node, struct custom_task, node);
            rb_erase(node, &cpu_rqs[cpu].tasks_timeline);
            kfree(task);
        }
        spin_unlock(&cpu_rqs[cpu].lock);
    }

    kfree(cpu_rqs);
    printk(KERN_INFO "Advanced CFS-like scheduler removed\n");
}

module_init(custom_scheduler_init);
module_exit(custom_scheduler_exit);
