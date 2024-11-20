#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

/* STM32F103 Specific Configurations */
#define CPU_CLOCK_HZ         72000000    /* 72 MHz */
#define SYSTICK_HZ           1000        /* 1000 Hz = 1ms tick */
#define SYSTICK_PERIOD       (CPU_CLOCK_HZ / SYSTICK_HZ)

/* Task Configuration */
#define MAX_TASKS           8            /* Maximum number of tasks */
#define TASK_STACK_SIZE     256          /* Stack size per task in words */
#define IDLE_TASK_PRIORITY  0            /* Lowest priority */
#define MAX_PRIORITY        31           /* Highest priority */

/* Memory Management */
#define HEAP_SIZE           4096         /* Size of heap in bytes */
#define MIN_BLOCK_SIZE      16           /* Minimum allocation size */

/* System Protection */
#define USE_MUTEX           1            /* Enable mutex support */
#define USE_SEMAPHORE       1            /* Enable semaphore support */
#define USE_QUEUE           1            /* Enable queue support */

/* Debug Options */
#define USE_STACK_CHECK     1            /* Enable stack overflow checking */
#define USE_STATS          1            /* Enable statistics collection */

#endif /* RTOS_CONFIG_H */
