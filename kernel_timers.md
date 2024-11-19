# Linux Kernel Timer Mechanisms and Events

## 1. Timer Hardware Components

### 1.1 Hardware Clock Sources
- **PIT (Programmable Interval Timer)**
  - Legacy 8254 timer chip
  - Used in older systems
  - 1.193182 MHz frequency

- **HPET (High Precision Event Timer)**
  - Modern replacement for PIT
  - Multiple timer comparators
  - Typically 10+ MHz frequency

- **Local APIC Timer**
  - Per-CPU timer
  - Used for CPU-local events
  - Synchronized with TSC

- **TSC (Time Stamp Counter)**
  - CPU cycle counter
  - High resolution
  - Used for fine-grained timing

### 1.2 RTC (Real Time Clock)
- Battery-backed hardware clock
- Maintains wall time
- Persists across reboots
- Low resolution (seconds)

## 2. Timer Subsystem Architecture

### 2.1 Timer Wheel
```c
struct timer_base {
    struct hlist_head vectors[WHEEL_SIZE];
    unsigned long clk;
    unsigned long next_expiry;
};
```

- **Characteristics**:
  - Hierarchical timing wheel
  - O(1) insertion/removal
  - Multiple resolution levels
  - Efficient timer management

### 2.2 Timer Types
1. **High Resolution Timers (hrtimers)**
   - Nanosecond resolution
   - Hardware-based
   - Used for precise timing

2. **Regular Timers**
   - Jiffies-based
   - Software-based
   - Millisecond resolution

3. **Dynamic Ticks**
   - NO_HZ configuration
   - Power-saving feature
   - Tickless operation

## 3. Most Important Timer Events

### 3.1 System Timer Events
1. **Scheduler Tick**
   ```c
   static void scheduler_tick(void)
   {
       /* Update scheduling statistics */
       update_rq_clock(rq);
       /* Check for preemption */
       task_tick_fair(rq, curr);
       /* Update load averages */
       calc_global_load();
   }
   ```
   - Frequency: HZ times per second
   - Updates scheduler statistics
   - Handles task preemption
   - Updates load averages

2. **RCU Callbacks**
   ```c
   static void rcu_check_callbacks(void)
   {
       /* Process RCU callbacks */
       rcu_check_quiescent_state();
       /* Handle grace periods */
       rcu_process_callbacks();
   }
   ```
   - Processes RCU callbacks
   - Manages grace periods
   - Handles memory reclamation

3. **Watchdog Timer**
   ```c
   static void watchdog_timer_fn(void)
   {
       /* Check for system hangs */
       check_cpu_stall();
       /* Reset hardware watchdog */
       kick_watchdog();
   }
   ```
   - Detects system hangs
   - Monitors CPU health
   - Triggers panic on failure

### 3.2 Network Timer Events
1. **TCP Timers**
   ```c
   struct tcp_timer {
       struct timer_list retransmit_timer;
       struct timer_list delayed_ack_timer;
       struct timer_list keepalive_timer;
       struct timer_list fin_wait2_timer;
   };
   ```
   - Retransmission timeouts
   - Delayed ACKs
   - Keep-alive checks
   - Connection timeouts

2. **Netfilter Timeouts**
   - Connection tracking
   - NAT timeouts
   - State cleanup

### 3.3 Block Layer Timer Events
1. **I/O Timeout**
   ```c
   static void blk_timeout_work(struct work_struct *work)
   {
       /* Handle I/O timeout */
       if (timeout_expired)
           blk_handle_request_timeout();
   }
   ```
   - Request timeouts
   - Error handling
   - Queue management

2. **Request Merging Window**
   - Merge adjacent requests
   - Optimize I/O patterns
   - Improve performance

### 3.4 Memory Management Timer Events
1. **Page Aging**
   ```c
   static void page_aging_timer(void)
   {
       /* Age pages in LRU lists */
       shrink_active_list();
       /* Handle page reclaim */
       balance_pgdat();
   }
   ```
   - LRU list management
   - Page reclamation
   - Memory balancing

2. **Swap Writer**
   - Write dirty pages
   - Balance swap usage
   - Handle page-out

## 4. Timer Implementation Mechanisms

### 4.1 Timer Registration
```c
void setup_timer(struct timer_list *timer,
                void (*function)(struct timer_list *),
                unsigned long data)
{
    timer->function = function;
    timer->data = data;
    init_timer(timer);
}
```

### 4.2 Timer Expiry Processing
```c
static void run_timer_softirq(struct softirq_action *h)
{
    struct timer_base *base = this_cpu_ptr(&timer_bases[BASE_STD]);
    
    /* Process expired timers */
    while (time_after_eq(jiffies, base->clk)) {
        struct hlist_head *head = &base->vectors[base->clk & MASK];
        run_timers(head);
        base->clk++;
    }
}
```

### 4.3 High Resolution Timer Management
```c
struct hrtimer {
    struct timerqueue_node node;
    ktime_t _softexpires;
    enum hrtimer_restart (*function)(struct hrtimer *);
    struct hrtimer_clock_base *base;
    u8 state;
};
```

## 5. Timer Precision and Resolution

### 5.1 Jiffies Resolution
- HZ configuration (100-1000 Hz)
- Millisecond granularity
- System-wide tick

### 5.2 High Resolution
- Nanosecond precision
- Hardware dependent
- Per-CPU timers

## 6. Power Management Considerations

### 6.1 Dynamic Ticks
```c
static void tick_nohz_handler(void)
{
    /* Calculate next tick */
    next_tick = get_next_timer_interrupt();
    /* Program timer hardware */
    tick_program_event(next_tick);
}
```

### 6.2 Timer Deferral
- Coalesce timer events
- Reduce wake-ups
- Save power

## 7. Common Timer Usage Patterns

### 7.1 Periodic Timers
```c
static void setup_periodic_timer(void)
{
    struct timer_list *timer = &per_cpu(periodic_timer);
    
    timer->expires = jiffies + HZ;  /* 1 second */
    timer->function = periodic_function;
    add_timer(timer);
}
```

### 7.2 One-Shot Timers
```c
static void setup_oneshot_timer(unsigned long delay)
{
    struct timer_list timer;
    
    timer.expires = jiffies + delay;
    timer.function = oneshot_function;
    add_timer(&timer);
}
```

## 8. Debugging and Troubleshooting

### 8.1 Timer Statistics
- `/proc/timer_list`
- Timer statistics
- Expiry tracking

### 8.2 Common Issues
1. **Timer Starvation**
   - Long-running handlers
   - High system load
   - IRQ problems

2. **Timer Accuracy**
   - Hardware limitations
   - System load impact
   - Clock source issues

3. **Timer Overruns**
   - Missed deadlines
   - Cascade effects
   - System performance
