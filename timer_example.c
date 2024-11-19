#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

/*
 * Example 1: Basic Timer Usage
 * Shows how to use the standard timer interface
 */

/* Timer structure */
static struct timer_list my_timer;

/* Timer callback function */
static void timer_callback(struct timer_list *t)
{
    printk(KERN_INFO "Timer callback executed at jiffies=%lu\n", jiffies);
    
    /* Reschedule timer for periodic execution */
    mod_timer(&my_timer, jiffies + HZ);  /* HZ = ticks per second */
}

/*
 * Example 2: High-Resolution Timer Usage
 * Shows how to use hrtimers for nanosecond precision
 */

static struct hrtimer hr_timer;

/* High-resolution timer callback */
static enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
    ktime_t now = ktime_get();
    printk(KERN_INFO "HRTimer callback at %lld ns\n", ktime_to_ns(now));
    
    /* Reschedule timer */
    hrtimer_forward_now(timer, ns_to_ktime(1000000)); /* 1ms */
    return HRTIMER_RESTART;
}

/*
 * Timer Hardware Layer Interface
 * Demonstrates how timers interact with hardware
 */

/* Clock event device callback */
static int clock_event_callback(struct clock_event_device *dev)
{
    /* Update jiffies */
    jiffies++;
    
    /* Trigger timer softirq */
    raise_softirq(TIMER_SOFTIRQ);
    
    return 0;
}

/* Timer softirq handler */
static void timer_softirq_handler(struct softirq_action *h)
{
    struct timer_base *base = this_cpu_ptr(&timer_bases[BASE_STD]);
    
    /* Process all expired timers */
    while (time_after_eq(jiffies, base->clk)) {
        struct hlist_head *head = &base->vectors[base->clk & MASK];
        process_timeout(head);
        base->clk++;
    }
}

/*
 * Timer Initialization and Management
 */

static int __init timer_example_init(void)
{
    ktime_t ktime;
    
    /* Initialize regular timer */
    timer_setup(&my_timer, timer_callback, 0);
    /* Start timer to expire in 1 second */
    mod_timer(&my_timer, jiffies + HZ);
    
    /* Initialize high-resolution timer */
    ktime = ktime_set(0, 1000000); /* 1ms */
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = hrtimer_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
    
    return 0;
}

static void __exit timer_example_exit(void)
{
    /* Cleanup regular timer */
    del_timer_sync(&my_timer);
    
    /* Cleanup high-resolution timer */
    hrtimer_cancel(&hr_timer);
}

module_init(timer_example_init);
module_exit(timer_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example");
MODULE_DESCRIPTION("Timer Usage Example");

/*
 * Timer Implementation Details (Pseudo-code showing internal working)
 */

/*
 * Hardware Timer Interrupt Flow:
 * 1. Hardware timer generates interrupt
 * 2. CPU jumps to interrupt handler
 * 3. Handler updates system time
 * 4. Raises softirq for timer processing
 */
void timer_interrupt(void)
{
    /* Acknowledge interrupt */
    ack_timer_interrupt();
    
    /* Update jiffies */
    jiffies++;
    
    /* Update wall time */
    update_wall_time();
    
    /* Schedule timer processing */
    raise_softirq(TIMER_SOFTIRQ);
}

/*
 * Timer Wheel Implementation (Pseudo-code)
 * Shows how timers are organized internally
 */
struct timer_base {
    spinlock_t lock;
    struct hlist_head vectors[WHEEL_SIZE];
    unsigned long clk;
};

/* Add timer to timer wheel */
static void enqueue_timer(struct timer_base *base, struct timer_list *timer)
{
    unsigned long expires = timer->expires;
    unsigned long idx = expires - base->clk;
    struct hlist_head *vec;
    
    /* Calculate wheel slot */
    idx = calc_wheel_index(expires, base);
    vec = base->vectors + idx;
    
    /* Add to hash list */
    hlist_add_head(&timer->entry, vec);
}

/*
 * High-Resolution Timer Implementation (Pseudo-code)
 * Shows how hrtimers work internally
 */
struct hrtimer_clock_base {
    struct timerqueue_head active;
    ktime_t resolution;
    ktime_t (*get_time)(void);
};

/* Add hrtimer to red-black tree */
static void enqueue_hrtimer(struct hrtimer *timer)
{
    struct hrtimer_clock_base *base = timer->base;
    struct timerqueue_node *node = &timer->node;
    
    /* Add to red-black tree */
    timerqueue_add(&base->active, node);
    
    /* Reprogram hardware timer if needed */
    if (node == timerqueue_first(&base->active))
        reprogram_timer_hardware(node->expires);
}

/*
 * Timer Hardware Programming (Pseudo-code)
 * Shows how hardware timers are programmed
 */
static void reprogram_timer_hardware(ktime_t expires)
{
    u64 delta;
    unsigned long flags;
    
    local_irq_save(flags);
    
    /* Calculate time delta */
    delta = ktime_to_ns(expires) - ktime_to_ns(ktime_get());
    
    /* Program hardware timer */
    if (delta > 0) {
        if (delta < MIN_DELTA)
            delta = MIN_DELTA;
        program_hardware_timer(delta);
    }
    
    local_irq_restore(flags);
}

/*
 * Usage Example Explanations:
 *
 * 1. Regular Timer Flow:
 *    - User calls timer_setup() to initialize timer
 *    - mod_timer() adds timer to timer wheel
 *    - Hardware interrupt occurs
 *    - Softirq processes expired timers
 *    - Callback function executed
 *
 * 2. High-Resolution Timer Flow:
 *    - User calls hrtimer_init() to initialize timer
 *    - hrtimer_start() adds timer to rb-tree
 *    - Hardware programmed with next expiry
 *    - Hardware interrupt occurs
 *    - Callback executed directly in interrupt context
 *
 * 3. Timer Wheel Structure:
 *    - Multiple levels of wheel
 *    - Each slot is a hash list
 *    - O(1) insertion and removal
 *    - Efficient timer management
 *
 * 4. Hardware Interaction:
 *    - Programs hardware countdown
 *    - Handles interrupts
 *    - Updates system time
 *    - Triggers software processing
 */
