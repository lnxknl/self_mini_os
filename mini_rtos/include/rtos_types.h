#ifndef RTOS_TYPES_H
#define RTOS_TYPES_H

#include <stdint.h>
#include "rtos_config.h"

/* Task States */
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED,
    TASK_DELETED
} task_state_t;

/* Task Control Block */
typedef struct tcb {
    uint32_t *sp;                   /* Stack pointer */
    uint32_t *stack_base;           /* Base of stack */
    uint32_t stack_size;            /* Stack size in words */
    void (*task_func)(void *);      /* Task function */
    void *arg;                      /* Task argument */
    uint8_t priority;               /* Task priority */
    task_state_t state;             /* Task state */
    uint32_t time_slice;            /* Time slice for round-robin */
    uint32_t ticks_remaining;       /* Ticks until timeout */
    struct tcb *next;               /* Next TCB in list */
    struct tcb *prev;               /* Previous TCB in list */
    #if USE_MUTEX
    struct mutex *mutex_waiting;    /* Mutex task is waiting for */
    #endif
    #if USE_STATS
    uint32_t cpu_usage;            /* CPU usage percentage */
    uint32_t stack_usage;          /* Stack usage in bytes */
    #endif
    char name[16];                 /* Task name */
} tcb_t;

/* Mutex Control Block */
#if USE_MUTEX
typedef struct mutex {
    uint8_t locked;                /* 0: unlocked, 1: locked */
    tcb_t *owner;                  /* Task that owns the mutex */
    tcb_t *waiting_list;           /* Tasks waiting for mutex */
} mutex_t;
#endif

/* Semaphore Control Block */
#if USE_SEMAPHORE
typedef struct semaphore {
    int32_t count;                /* Semaphore count */
    tcb_t *waiting_list;          /* Tasks waiting for semaphore */
} semaphore_t;
#endif

/* Queue Control Block */
#if USE_QUEUE
typedef struct queue {
    void *buffer;                 /* Queue buffer */
    uint32_t size;               /* Queue size in bytes */
    uint32_t item_size;          /* Size of each item */
    uint32_t count;              /* Number of items in queue */
    uint32_t head;               /* Index of first item */
    uint32_t tail;               /* Index of last item */
    tcb_t *waiting_send;         /* Tasks waiting to send */
    tcb_t *waiting_receive;      /* Tasks waiting to receive */
} queue_t;
#endif

/* Memory Block Header */
typedef struct mem_block {
    uint32_t size;               /* Size of block including header */
    uint8_t free;                /* 1 if block is free, 0 if used */
    struct mem_block *next;      /* Next block in list */
} mem_block_t;

/* System Statistics */
#if USE_STATS
typedef struct {
    uint32_t task_switches;      /* Number of task switches */
    uint32_t cpu_usage;          /* System CPU usage */
    uint32_t heap_usage;         /* Heap usage in bytes */
    uint32_t stack_usage;        /* Stack usage in bytes */
} system_stats_t;
#endif

/* Function Types */
typedef void (*task_function_t)(void *arg);
typedef void (*isr_function_t)(void);

#endif /* RTOS_TYPES_H */
