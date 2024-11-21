#include "timer.h"
#include "rtos_core.h"
#include "stm32f10x.h"

/* Global Variables */
static timer_base_t timer_bases[MAX_CPU];
static timer_stats_t global_stats;

/* Timer Wheel Helper Functions */
static uint32_t get_wheel_index(uint32_t expires, uint8_t level) {
    return (expires >> (TIMER_WHEEL_SIZE * level)) & TIMER_WHEEL_MASK;
}

static void cascade_timers(timer_wheel_t *wheel, uint8_t level) {
    uint32_t index = get_wheel_index(wheel->current_tick, level);
    timer_t *timer, *next;
    
    timer = wheel->wheel[level][index];
    wheel->wheel[level][index] = NULL;
    
    while (timer) {
        next = timer->next;
        
        /* Re-add timer to appropriate level */
        if (level > 0) {
            timer_wheel_add(wheel, timer);
        }
        
        timer = next;
    }
}

/* Timer Wheel Implementation */
void timer_wheel_add(timer_wheel_t *wheel, timer_t *timer) {
    uint32_t expires = timer->expires;
    uint32_t idx = get_wheel_index(expires, 0);
    uint8_t level = 0;
    
    /* Find appropriate wheel level */
    while (expires >> (TIMER_WHEEL_SIZE * (level + 1))) {
        level++;
        if (level >= TIMER_WHEEL_LEVELS) {
            level = TIMER_WHEEL_LEVELS - 1;
            break;
        }
    }
    
    idx = get_wheel_index(expires, level);
    
    /* Add to wheel */
    enter_critical();
    
    timer->next = wheel->wheel[level][idx];
    if (wheel->wheel[level][idx]) {
        wheel->wheel[level][idx]->prev = timer;
    }
    wheel->wheel[level][idx] = timer;
    timer->state = TIMER_ENQUEUED;
    
    exit_critical();
}

void timer_wheel_remove(timer_wheel_t *wheel, timer_t *timer) {
    enter_critical();
    
    if (timer->prev) {
        timer->prev->next = timer->next;
    } else {
        uint32_t idx = get_wheel_index(timer->expires, 0);
        uint8_t level;
        
        /* Find the wheel level */
        for (level = 0; level < TIMER_WHEEL_LEVELS; level++) {
            if (wheel->wheel[level][idx] == timer) {
                wheel->wheel[level][idx] = timer->next;
                break;
            }
            idx = get_wheel_index(timer->expires, level + 1);
        }
    }
    
    if (timer->next) {
        timer->next->prev = timer->prev;
    }
    
    timer->state = TIMER_INACTIVE;
    timer->next = timer->prev = NULL;
    
    exit_critical();
}

void timer_wheel_advance(timer_wheel_t *wheel) {
    uint32_t index = wheel->current_tick & TIMER_WHEEL_MASK;
    
    /* Process expired timers */
    timer_t *timer = wheel->wheel[0][index];
    wheel->wheel[0][index] = NULL;
    
    while (timer) {
        timer_t *next = timer->next;
        
        if (timer->expires <= wheel->current_tick) {
            /* Timer expired */
            timer->state = TIMER_EXPIRED;
            
            /* Execute callback */
            if (timer->callback) {
                uint32_t start = timer_get_time_ns();
                timer->callback(timer->callback_arg);
                uint32_t latency = timer_get_time_ns() - start;
                
                /* Update statistics */
                global_stats.total_latency += latency;
                if (latency > global_stats.max_latency) {
                    global_stats.max_latency = latency;
                }
                if (latency < global_stats.min_latency) {
                    global_stats.min_latency = latency;
                }
                global_stats.expired_timers++;
            }
            
            /* Handle periodic timers */
            if (timer->type == TIMER_PERIODIC) {
                timer->expires += timer->period;
                timer_wheel_add(wheel, timer);
            } else {
                timer->state = TIMER_INACTIVE;
            }
        } else {
            /* Timer not expired, re-add to wheel */
            timer_wheel_add(wheel, timer);
        }
        
        timer = next;
    }
    
    /* Cascade timers from upper levels if needed */
    if ((wheel->current_tick & ((1 << TIMER_WHEEL_SIZE) - 1)) == 0) {
        cascade_timers(wheel, 1);
        
        if ((wheel->current_tick & ((1 << (2 * TIMER_WHEEL_SIZE)) - 1)) == 0) {
            cascade_timers(wheel, 2);
            
            if ((wheel->current_tick & ((1 << (3 * TIMER_WHEEL_SIZE)) - 1)) == 0) {
                cascade_timers(wheel, 3);
            }
        }
    }
    
    wheel->current_tick++;
}

/* Timer Base Functions */
void timer_base_advance(timer_base_t *base) {
    if (!base->active) {
        return;
    }
    
    timer_wheel_advance(&base->wheel);
    
    /* Update next expiry */
    base->next_expiry = timer_base_get_next_expiry(base);
}

uint32_t timer_base_get_next_expiry(timer_base_t *base) {
    uint32_t next_expiry = UINT32_MAX;
    uint8_t level, idx;
    
    for (level = 0; level < TIMER_WHEEL_LEVELS; level++) {
        for (idx = 0; idx < TIMER_WHEEL_SIZE; idx++) {
            timer_t *timer = base->wheel.wheel[level][idx];
            while (timer) {
                if (timer->expires < next_expiry) {
                    next_expiry = timer->expires;
                }
                timer = timer->next;
            }
        }
    }
    
    return next_expiry;
}

/* Timer Management Functions */
timer_t *timer_create(timer_type_t type, uint32_t flags) {
    timer_t *timer = rtos_malloc(sizeof(timer_t));
    if (!timer) {
        return NULL;
    }
    
    timer->type = type;
    timer->flags = flags;
    timer->state = TIMER_INACTIVE;
    timer->next = timer->prev = NULL;
    timer->cpu = get_current_cpu();
    
    global_stats.total_timers++;
    if (flags & TIMER_FLAG_HIGH_RES) {
        global_stats.high_res_timers++;
    }
    if (flags & TIMER_FLAG_DEFERRABLE) {
        global_stats.deferrable_timers++;
    }
    
    return timer;
}

int timer_start(timer_t *timer, uint32_t expires, uint32_t period,
                timer_callback_t callback, void *arg) {
    if (!timer || !callback) {
        return -1;
    }
    
    timer->expires = expires;
    timer->period = period;
    timer->callback = callback;
    timer->callback_arg = arg;
    
    timer_base_t *base = &timer_bases[timer->cpu];
    timer_wheel_add(&base->wheel, timer);
    
    global_stats.active_timers++;
    
    return 0;
}

int timer_stop(timer_t *timer) {
    if (!timer) {
        return -1;
    }
    
    timer_base_t *base = &timer_bases[timer->cpu];
    timer_wheel_remove(&base->wheel, timer);
    
    global_stats.active_timers--;
    
    return 0;
}

/* High Resolution Timer Functions */
timer_t *hrtimer_create(uint32_t flags) {
    return timer_create(TIMER_HIGH_RES, flags | TIMER_FLAG_HIGH_RES);
}

int hrtimer_start(timer_t *timer, uint64_t expires_ns,
                  timer_callback_t callback, void *arg) {
    /* Convert nanoseconds to timer ticks */
    uint32_t expires = (uint32_t)(expires_ns / timer_bases[timer->cpu].resolution);
    return timer_start(timer, expires, 0, callback, arg);
}

/* Timer Subsystem Initialization */
void timer_subsystem_init(void) {
    uint8_t cpu;
    
    /* Initialize timer bases for each CPU */
    for (cpu = 0; cpu < MAX_CPU; cpu++) {
        timer_base_t *base = &timer_bases[cpu];
        base->cpu = cpu;
        base->resolution = 1000; /* 1 microsecond resolution */
        base->active = 1;
        
        /* Initialize wheel */
        memset(&base->wheel, 0, sizeof(timer_wheel_t));
        base->wheel.resolution = base->resolution;
        base->wheel.lock = mutex_create();
    }
    
    /* Initialize statistics */
    memset(&global_stats, 0, sizeof(timer_stats_t));
    global_stats.min_latency = UINT32_MAX;
}

/* Timer Statistics Functions */
void get_timer_stats(timer_stats_t *stats) {
    if (stats) {
        memcpy(stats, &global_stats, sizeof(timer_stats_t));
    }
}

void reset_timer_stats(void) {
    memset(&global_stats, 0, sizeof(timer_stats_t));
    global_stats.min_latency = UINT32_MAX;
}
