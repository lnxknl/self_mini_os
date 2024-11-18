#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/time.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom CFS Scheduler");
MODULE_DESCRIPTION("A Linux CFS-like task scheduler implementation");

// Task states
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

// Task structure
struct custom_task {
    pid_t pid;
    int state;
    int priority;
    int nice;
    unsigned int weight;
    unsigned long long vruntime;    // Virtual runtime in ns
    unsigned long long exec_start;   // Start of execution
    unsigned long long sum_exec_runtime; // Total runtime
    struct rb_node node;           // For red-black tree
    struct list_head list;         // For task lists
};

// Global variables
static struct rb_root cfs_rbtree = RB_ROOT;
static LIST_HEAD(blocked_queue);
static struct custom_task *current_task = NULL;
static unsigned long long min_vruntime = 0;
static unsigned long total_weight = 0;

// Calculate task's weight based on nice value
static unsigned int calc_weight(int nice) {
    const int prio_to_weight[40] = {
        /* -20 */ 88761, 71755, 56483, 46273, 36291,
        /* -15 */ 29154, 23254, 18705, 14949, 11916,
        /* -10 */ 9548, 7620, 6100, 4904, 3906,
        /*  -5 */ 3121, 2501, 1991, 1586, 1277,
        /*   0 */ 1024, 820, 655, 526, 423,
        /*   5 */ 335, 272, 215, 172, 137,
        /*  10 */ 110, 87, 70, 56, 45,
        /*  15 */ 36, 29, 23, 18, 15,
    };
    
    nice += 20;
    if (nice < 0)
        nice = 0;
    if (nice > 39)
        nice = 39;
    
    return prio_to_weight[nice];
}

// Initialize a new task
static struct custom_task *create_task(pid_t pid, int nice) {
    struct custom_task *task = kmalloc(sizeof(struct custom_task), GFP_KERNEL);
    if (!task)
        return NULL;

    task->pid = pid;
    task->state = TASK_READY;
    task->nice = nice;
    task->weight = calc_weight(nice);
    task->vruntime = min_vruntime;
    task->exec_start = 0;
    task->sum_exec_runtime = 0;
    RB_CLEAR_NODE(&task->node);
    INIT_LIST_HEAD(&task->list);

    total_weight += task->weight;
    return task;
}

// Insert task into red-black tree
static void insert_task_rb(struct custom_task *task) {
    struct rb_node **new = &cfs_rbtree.rb_node;
    struct rb_node *parent = NULL;

    while (*new) {
        struct custom_task *this = container_of(*new, struct custom_task, node);
        parent = *new;

        if (task->vruntime < this->vruntime)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }

    rb_link_node(&task->node, parent, new);
    rb_insert_color(&task->node, &cfs_rbtree);
}

// Remove task from red-black tree
static void remove_task_rb(struct custom_task *task) {
    if (!RB_EMPTY_NODE(&task->node)) {
        rb_erase(&task->node, &cfs_rbtree);
        RB_CLEAR_NODE(&task->node);
    }
}

// Find leftmost (smallest vruntime) task
static struct custom_task *pick_next_task_fair(void) {
    struct rb_node *leftmost = rb_first(&cfs_rbtree);
    if (!leftmost)
        return NULL;
    
    return container_of(leftmost, struct custom_task, node);
}

// Update task's vruntime
static void update_curr(struct custom_task *task) {
    unsigned long long now = ktime_get_ns();
    unsigned long long delta_exec;
    unsigned long long delta_fair;
    
    if (!task->exec_start)
        return;

    delta_exec = now - task->exec_start;
    task->sum_exec_runtime += delta_exec;

    // Calculate fair delta based on weight
    delta_fair = delta_exec * NICE_TO_WEIGHT(0) / task->weight;
    task->vruntime += delta_fair;

    // Update minimum vruntime
    if (task->vruntime < min_vruntime)
        min_vruntime = task->vruntime;
}

// Block a task
static void block_task(struct custom_task *task) {
    if (!task)
        return;

    update_curr(task);
    remove_task_rb(task);
    task->state = TASK_BLOCKED;
    list_add_tail(&task->list, &blocked_queue);
    total_weight -= task->weight;
}

// Unblock a task
static void unblock_task(struct custom_task *task) {
    if (!task)
        return;

    list_del(&task->list);
    task->state = TASK_READY;
    task->vruntime = min_vruntime;
    insert_task_rb(task);
    total_weight += task->weight;
}

// Calculate next preemption
static unsigned long calc_delta_fair(unsigned long delta) {
    unsigned long fair_delta;
    
    if (unlikely(total_weight < NICE_TO_WEIGHT(0)))
        total_weight = NICE_TO_WEIGHT(0);

    fair_delta = (delta * total_weight) / NICE_TO_WEIGHT(0);
    if (fair_delta < MIN_GRANULARITY)
        fair_delta = MIN_GRANULARITY;

    return fair_delta;
}

// Main scheduler function
static void schedule(void) {
    struct custom_task *prev, *next;
    unsigned long long now;

    prev = current_task;
    if (prev) {
        update_curr(prev);
        if (prev->state == TASK_RUNNING) {
            prev->state = TASK_READY;
            insert_task_rb(prev);
        }
    }

    next = pick_next_task_fair();
    if (!next) {
        printk(KERN_INFO "No tasks to schedule\n");
        return;
    }

    remove_task_rb(next);
    now = ktime_get_ns();
    next->exec_start = now;
    next->state = TASK_RUNNING;
    current_task = next;

    printk(KERN_INFO "Scheduling task PID: %d, vruntime: %llu\n", 
           next->pid, next->vruntime);
}

// API functions
int custom_create_task(pid_t pid, int nice) {
    struct custom_task *task;

    if (nice < -20 || nice > 19)
        return -EINVAL;

    task = create_task(pid, nice);
    if (!task)
        return -ENOMEM;

    insert_task_rb(task);
    return 0;
}
EXPORT_SYMBOL(custom_create_task);

void custom_block_task(pid_t pid) {
    struct rb_node *node;
    struct custom_task *task;

    for (node = rb_first(&cfs_rbtree); node; node = rb_next(node)) {
        task = container_of(node, struct custom_task, node);
        if (task->pid == pid) {
            block_task(task);
            break;
        }
    }
}
EXPORT_SYMBOL(custom_block_task);

void custom_unblock_task(pid_t pid) {
    struct custom_task *task;

    list_for_each_entry(task, &blocked_queue, list) {
        if (task->pid == pid) {
            unblock_task(task);
            break;
        }
    }
}
EXPORT_SYMBOL(custom_unblock_task);

// Module initialization
static int __init custom_scheduler_init(void) {
    printk(KERN_INFO "CFS-like scheduler initialized\n");
    return 0;
}

// Module cleanup
static void __exit custom_scheduler_exit(void) {
    struct rb_node *node;
    struct custom_task *task, *tmp;

    // Clean up red-black tree
    while ((node = rb_first(&cfs_rbtree))) {
        task = container_of(node, struct custom_task, node);
        remove_task_rb(task);
        kfree(task);
    }

    // Clean up blocked queue
    list_for_each_entry_safe(task, tmp, &blocked_queue, list) {
        list_del(&task->list);
        kfree(task);
    }

    printk(KERN_INFO "CFS-like scheduler removed\n");
}

module_init(custom_scheduler_init);
module_exit(custom_scheduler_exit);
