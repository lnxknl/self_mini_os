#include <linux/sched.h>
#include <linux/sched/deadline.h>
#include <linux/sched/rt.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/ktime.h>

/*
 * Preemption Control Implementation
 * Demonstrates how the kernel manages task preemption
 */

/* Preemption counter for nested preempt disable */
DEFINE_PER_CPU(int, preempt_count);

void custom_preempt_disable(void)
{
    /* Increment preemption counter */
    preempt_count_inc();
    barrier();  /* Prevent reordering */
}

void custom_preempt_enable(void)
{
    barrier();  /* Prevent reordering */
    if (unlikely(!preempt_count_dec_and_test()))
        return;
    
    /* Check if we need to preempt */
    if (unlikely(need_resched()))
        preempt_schedule();
}

/*
 * Real-time Task Implementation
 * Shows how real-time tasks are managed
 */

struct rt_task_struct {
    struct task_struct *task;
    int rt_priority;
    u64 deadline;
    spinlock_t lock;
    struct list_head run_list;
};

/* Initialize a real-time task */
int init_rt_task(struct rt_task_struct *rt_task, struct task_struct *task, int priority)
{
    if (priority < 0 || priority > MAX_RT_PRIO - 1)
        return -EINVAL;

    spin_lock_init(&rt_task->lock);
    rt_task->task = task;
    rt_task->rt_priority = priority;
    INIT_LIST_HEAD(&rt_task->run_list);

    /* Set real-time scheduling class */
    task->sched_class = &rt_sched_class;
    task->policy = SCHED_FIFO;
    task->rt_priority = priority;

    return 0;
}

/*
 * Deadline Scheduling Implementation
 * Demonstrates EDF (Earliest Deadline First) scheduling
 */

struct dl_task_struct {
    struct task_struct *task;
    u64 deadline;
    u64 runtime;
    u64 period;
    struct rb_node node;  /* For deadline rbtree */
};

/* Initialize a deadline scheduled task */
int init_dl_task(struct dl_task_struct *dl_task, struct task_struct *task,
                 u64 runtime, u64 deadline, u64 period)
{
    if (runtime > deadline || deadline > period)
        return -EINVAL;

    dl_task->task = task;
    dl_task->runtime = runtime;
    dl_task->deadline = deadline;
    dl_task->period = period;

    /* Set deadline scheduling class */
    task->sched_class = &dl_sched_class;
    task->policy = SCHED_DEADLINE;

    return 0;
}

/* Add task to deadline rbtree */
void add_dl_task_to_rbtree(struct dl_task_struct *dl_task, struct rb_root *root)
{
    struct rb_node **new = &root->rb_node, *parent = NULL;

    while (*new) {
        struct dl_task_struct *this;
        parent = *new;
        
        this = rb_entry(parent, struct dl_task_struct, node);
        if (dl_task->deadline < this->deadline)
            new = &parent->rb_left;
        else
            new = &parent->rb_right;
    }

    rb_link_node(&dl_task->node, parent, new);
    rb_insert_color(&dl_task->node, root);
}

/*
 * Priority Inheritance Implementation
 * Shows how priority inheritance prevents priority inversion
 */

struct pi_mutex {
    struct task_struct *owner;
    spinlock_t lock;
    struct list_head waiters;
    int orig_prio;
};

/* Initialize a priority inheritance mutex */
void init_pi_mutex(struct pi_mutex *pi_mutex)
{
    spin_lock_init(&pi_mutex->lock);
    pi_mutex->owner = NULL;
    INIT_LIST_HEAD(&pi_mutex->waiters);
    pi_mutex->orig_prio = -1;
}

/* Lock a PI mutex with priority inheritance */
void pi_mutex_lock(struct pi_mutex *pi_mutex)
{
    struct task_struct *task = current;
    unsigned long flags;

    spin_lock_irqsave(&pi_mutex->lock, flags);

    if (pi_mutex->owner == NULL) {
        /* Lock is free, take it */
        pi_mutex->owner = task;
        pi_mutex->orig_prio = task->prio;
    } else {
        /* Lock is taken, implement priority inheritance */
        struct task_struct *owner = pi_mutex->owner;
        
        /* Add ourselves to waiter list */
        list_add_tail(&task->pi_waiters, &pi_mutex->waiters);
        
        /* Implement priority inheritance if needed */
        if (task->prio < owner->prio) {
            /* Boost the owner's priority */
            owner->prio = task->prio;
            /* Propagate priority boost if owner is blocked */
            if (owner->state & TASK_UNINTERRUPTIBLE)
                propagate_priority_inheritance(owner);
        }
        
        spin_unlock_irqrestore(&pi_mutex->lock, flags);
        
        /* Wait for lock */
        for (;;) {
            set_current_state(TASK_UNINTERRUPTIBLE);
            if (pi_mutex->owner == NULL)
                break;
            schedule();
        }
        
        spin_lock_irqsave(&pi_mutex->lock, flags);
        pi_mutex->owner = task;
    }
    
    spin_unlock_irqrestore(&pi_mutex->lock, flags);
}

/* Unlock a PI mutex */
void pi_mutex_unlock(struct pi_mutex *pi_mutex)
{
    unsigned long flags;
    struct task_struct *next = NULL;

    spin_lock_irqsave(&pi_mutex->lock, flags);

    /* Restore original priority */
    current->prio = pi_mutex->orig_prio;

    /* Find highest priority waiter */
    if (!list_empty(&pi_mutex->waiters)) {
        next = list_first_entry(&pi_mutex->waiters,
                              struct task_struct, pi_waiters);
        list_del(&next->pi_waiters);
        wake_up_process(next);
    }

    pi_mutex->owner = NULL;
    spin_unlock_irqrestore(&pi_mutex->lock, flags);
}

/*
 * Helper function to propagate priority inheritance
 * through a chain of locks
 */
static void propagate_priority_inheritance(struct task_struct *task)
{
    struct task_struct *owner;
    struct pi_mutex *pi_mutex;

    /* Follow the chain of locks */
    while ((pi_mutex = task->blocked_on) != NULL) {
        owner = pi_mutex->owner;
        if (!owner || owner->prio <= task->prio)
            break;
        
        /* Propagate the priority boost */
        owner->prio = task->prio;
        task = owner;
    }
}

/*
 * Scheduler tick handler for deadline tasks
 * Updates deadlines and handles overruns
 */
void deadline_tick(struct dl_task_struct *dl_task)
{
    u64 now = ktime_get_ns();

    if (now >= dl_task->deadline) {
        /* Deadline missed - handle overrun */
        if (dl_task->task->policy == SCHED_DEADLINE) {
            /* Replenish runtime and set new deadline */
            dl_task->deadline += dl_task->period;
            dl_task->runtime = dl_task->task->dl.runtime;
            
            /* Optional: Log deadline miss */
            printk(KERN_WARNING "Task %d missed deadline\n",
                   dl_task->task->pid);
        }
    }
}

/*
 * Example usage of the scheduling features
 */
void example_scheduling_usage(void)
{
    struct task_struct *task;
    struct rt_task_struct rt_task;
    struct dl_task_struct dl_task;
    struct pi_mutex pi_mutex;
    
    /* Create a real-time task */
    task = current;
    init_rt_task(&rt_task, task, 1);  /* Priority 1 (high) */
    
    /* Create a deadline task */
    init_dl_task(&dl_task, task,
                 100000000,    /* Runtime: 100ms */
                 200000000,    /* Deadline: 200ms */
                 1000000000);  /* Period: 1s */
    
    /* Use priority inheritance mutex */
    init_pi_mutex(&pi_mutex);
    pi_mutex_lock(&pi_mutex);
    /* Critical section */
    pi_mutex_unlock(&pi_mutex);
    
    /* Disable preemption for critical section */
    custom_preempt_disable();
    /* Critical section */
    custom_preempt_enable();
}
