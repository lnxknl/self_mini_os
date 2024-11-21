#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "rtos_types.h"

/* Timer Types */
typedef enum {
    TIMER_ONESHOT = 0,
    TIMER_PERIODIC,
    TIMER_HIGH_RES,
    TIMER_CPU_SPECIFIC,
    TIMER_DEFERRABLE    /* Can be deferred during system sleep */
} timer_type_t;

/* Timer States */
typedef enum {
    TIMER_INACTIVE = 0,
    TIMER_ENQUEUED,
    TIMER_RUNNING,
    TIMER_EXPIRED,
    TIMER_CANCELLED
} timer_state_t;

/* Timer Flags */
#define TIMER_FLAG_HARD_DEADLINE  (1 << 0)  /* Must execute at exact time */
#define TIMER_FLAG_SOFT_DEADLINE  (1 << 1)  /* Can be slightly delayed */
#define TIMER_FLAG_NO_MIGRATE    (1 << 2)  /* Don't migrate between CPUs */
#define TIMER_FLAG_DEFERRABLE    (1 << 3)  /* Can be deferred during sleep */
#define TIMER_FLAG_PINNED        (1 << 4)  /* Pinned to specific CPU */
#define TIMER_FLAG_HIGH_RES      (1 << 5)  /* High resolution timer */

/* Timer Callback Function */
typedef void (*timer_callback_t)(void *arg);

/* Timer Structure */
typedef struct timer {
    uint32_t expires;           /* Expiration time in ticks */
    uint32_t period;            /* Period for periodic timers */
    timer_callback_t callback;  /* Callback function */
    void *callback_arg;         /* Argument to callback */
    timer_type_t type;         /* Timer type */
    timer_state_t state;       /* Current state */
    uint32_t flags;            /* Timer flags */
    uint8_t cpu;               /* CPU affinity */
    uint8_t priority;          /* Timer priority */
    struct timer *next;        /* Next timer in wheel */
    struct timer *prev;        /* Previous timer in wheel */
} timer_t;

/* Timer Wheel Structure */
#define TIMER_WHEEL_SIZE     8
#define TIMER_WHEEL_MASK     (TIMER_WHEEL_SIZE - 1)
#define TIMER_WHEEL_LEVELS   4

typedef struct {
    timer_t *wheel[TIMER_WHEEL_LEVELS][TIMER_WHEEL_SIZE];
    uint32_t current_tick;
    uint32_t resolution;      /* Timer resolution in microseconds */
    mutex_t *lock;
} timer_wheel_t;

/* Timer Base Structure */
typedef struct {
    timer_wheel_t wheel;
    uint32_t next_expiry;
    uint32_t resolution;
    uint8_t cpu;
    uint8_t active;
} timer_base_t;

/* Timer Statistics */
typedef struct {
    uint32_t total_timers;
    uint32_t active_timers;
    uint32_t expired_timers;
    uint32_t overrun_timers;
    uint32_t high_res_timers;
    uint32_t deferrable_timers;
    uint64_t total_latency;    /* Total callback latency in microseconds */
    uint32_t max_latency;      /* Maximum callback latency */
    uint32_t min_latency;      /* Minimum callback latency */
} timer_stats_t;

/* Timer Subsystem Functions */

/* Initialization */
void timer_subsystem_init(void);
timer_base_t *timer_base_create(uint8_t cpu, uint32_t resolution);
void timer_base_destroy(timer_base_t *base);

/* Timer Management */
timer_t *timer_create(timer_type_t type, uint32_t flags);
void timer_destroy(timer_t *timer);
int timer_start(timer_t *timer, uint32_t expires, uint32_t period,
                timer_callback_t callback, void *arg);
int timer_stop(timer_t *timer);
int timer_modify(timer_t *timer, uint32_t new_expires);

/* High Resolution Timer Functions */
timer_t *hrtimer_create(uint32_t flags);
int hrtimer_start(timer_t *timer, uint64_t expires_ns,
                  timer_callback_t callback, void *arg);
int hrtimer_cancel(timer_t *timer);

/* Timer Wheel Management */
void timer_wheel_add(timer_wheel_t *wheel, timer_t *timer);
void timer_wheel_remove(timer_wheel_t *wheel, timer_t *timer);
void timer_wheel_advance(timer_wheel_t *wheel);
timer_t *timer_wheel_get_expired(timer_wheel_t *wheel);

/* Timer Base Management */
void timer_base_advance(timer_base_t *base);
uint32_t timer_base_get_next_expiry(timer_base_t *base);
void timer_base_process_events(timer_base_t *base);

/* Timer Statistics and Debugging */
void get_timer_stats(timer_stats_t *stats);
void reset_timer_stats(void);
void dump_timer_info(timer_t *timer);
void dump_timer_wheel(timer_wheel_t *wheel);

/* Timer Utility Functions */
uint32_t timer_get_resolution(void);
uint64_t timer_get_time_ns(void);
void timer_delay_us(uint32_t microseconds);
uint32_t timer_elapsed(uint32_t start_tick);

#endif /* TIMER_H */
