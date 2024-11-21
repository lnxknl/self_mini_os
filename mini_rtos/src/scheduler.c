#include "scheduler.h"
#include "rtos_core.h"
#include "stm32f10x.h"
#include "timer.h"
#include <string.h>

/* Scheduler State */
static tcb_t *current_task = NULL;
static tcb_t *next_task = NULL;
static uint32_t task_count = 0;
static scheduler_stats_t stats;

/* Priority Queue for Ready Tasks */
static tcb_t *ready_queue[MAX_PRIORITY_LEVELS];
static uint32_t ready_groups = 0;
static uint32_t ready_table[32];

/* Task Lists */
static tcb_t *blocked_list = NULL;
static tcb_t *suspended_list = NULL;
static tcb_t task_pool[MAX_TASKS];
static uint32_t task_stack_pool[MAX_TASKS][TASK_STACK_SIZE/4];

/* Idle Task */
static tcb_t *idle_task;
static uint32_t idle_stack[IDLE_TASK_STACK_SIZE/4];

/* Initialize Task Stack */
static void *init_task_stack(void *stack_top, void (*entry)(void *), void *arg) {
    uint32_t *stk = (uint32_t *)stack_top;
    
    /* Exception return frame */
    *(--stk) = 0x01000000;    /* xPSR */
    *(--stk) = (uint32_t)entry;/* PC */
    *(--stk) = 0;             /* LR */
    *(--stk) = 0;             /* R12 */
    *(--stk) = 0;             /* R3 */
    *(--stk) = 0;             /* R2 */
    *(--stk) = 0;             /* R1 */
    *(--stk) = (uint32_t)arg; /* R0 */
    
    /* Additional registers */
    *(--stk) = 0;             /* R11 */
    *(--stk) = 0;             /* R10 */
    *(--stk) = 0;             /* R9 */
    *(--stk) = 0;             /* R8 */
    *(--stk) = 0;             /* R7 */
    *(--stk) = 0;             /* R6 */
    *(--stk) = 0;             /* R5 */
    *(--stk) = 0;             /* R4 */
    
    return stk;
}

/* Idle Task Function */
static void idle_task_func(void *arg) {
    while (1) {
        /* Put CPU into low power mode */
        __WFI();
    }
}

/* Initialize Scheduler */
void scheduler_init(void) {
    /* Clear task structures */
    memset(task_pool, 0, sizeof(task_pool));
    memset(&stats, 0, sizeof(stats));
    
    /* Clear ready queue */
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        ready_queue[i] = NULL;
    }
    
    /* Initialize ready table */
    for (int i = 0; i < 32; i++) {
        ready_table[i] = 0;
    }
    ready_groups = 0;
    
    /* Create idle task */
    idle_task = task_create("idle", idle_task_func, NULL, 
                           TASK_PRIORITY_IDLE, IDLE_TASK_STACK_SIZE);
}

/* Task Creation */
tcb_t *task_create(const char *name, void (*entry)(void *), void *arg,
                   uint8_t priority, uint32_t stack_size) {
    if (task_count >= MAX_TASKS || !entry || priority >= MAX_PRIORITY_LEVELS) {
        return NULL;
    }
    
    /* Find free TCB */
    tcb_t *task = NULL;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_TERMINATED) {
            task = &task_pool[i];
            break;
        }
    }
    if (!task) return NULL;
    
    /* Initialize TCB */
    memset(task, 0, sizeof(tcb_t));
    strncpy(task->name, name, sizeof(task->name)-1);
    task->id = task_count++;
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = TASK_SWITCH_TICKS;
    task->ticks_remaining = TASK_SWITCH_TICKS;
    task->entry_point = entry;
    task->arg = arg;
    
    /* Initialize stack */
    task->stack_size = stack_size;
    task->stack_base = &task_stack_pool[task->id][0];
    task->stack_ptr = init_task_stack(
        (void *)((uint32_t)task->stack_base + stack_size),
        entry, arg
    );
    
    /* Add to ready queue */
    scheduler_add_task(task);
    stats.total_tasks++;
    
    return task;
}

/* Task State Management */
void task_suspend(tcb_t *task) {
    if (!task || task->state == TASK_SUSPENDED) return;
    
    enter_critical();
    
    /* Remove from current queue */
    if (task->state == TASK_READY) {
        scheduler_remove_task(task);
    } else if (task->state == TASK_BLOCKED) {
        /* Remove from blocked list */
        if (task->prev) task->prev->next = task->next;
        if (task->next) task->next->prev = task->prev;
        if (blocked_list == task) blocked_list = task->next;
    }
    
    /* Add to suspended list */
    task->next = suspended_list;
    task->prev = NULL;
    if (suspended_list) suspended_list->prev = task;
    suspended_list = task;
    task->state = TASK_SUSPENDED;
    
    exit_critical();
    
    /* If current task is suspended, force switch */
    if (task == current_task) {
        scheduler_switch_task();
    }
}

void task_resume(tcb_t *task) {
    if (!task || task->state != TASK_SUSPENDED) return;
    
    enter_critical();
    
    /* Remove from suspended list */
    if (task->prev) task->prev->next = task->next;
    if (task->next) task->next->prev = task->prev;
    if (suspended_list == task) suspended_list = task->next;
    
    /* Add to ready queue */
    task->state = TASK_READY;
    scheduler_add_task(task);
    
    exit_critical();
}

/* Task Delay Functions */
void task_delay(uint32_t ticks) {
    if (ticks == 0) return;
    
    enter_critical();
    
    current_task->state = TASK_BLOCKED;
    current_task->deadline = get_system_ticks() + ticks;
    
    /* Add to blocked list */
    current_task->next = blocked_list;
    current_task->prev = NULL;
    if (blocked_list) blocked_list->prev = current_task;
    blocked_list = current_task;
    
    scheduler_remove_task(current_task);
    
    exit_critical();
    
    /* Force task switch */
    scheduler_switch_task();
}

/* Process Blocked Tasks */
void process_blocked_tasks(void) {
    tcb_t *task = blocked_list;
    uint32_t current_ticks = get_system_ticks();
    
    while (task) {
        tcb_t *next = task->next;
        
        if (current_ticks >= task->deadline) {
            /* Remove from blocked list */
            if (task->prev) task->prev->next = task->next;
            if (task->next) task->next->prev = task->prev;
            if (blocked_list == task) blocked_list = task->next;
            
            /* Add to ready queue */
            task->state = TASK_READY;
            scheduler_add_task(task);
        }
        
        task = next;
    }
}

/* Get Current Task */
tcb_t *get_current_task(void) {
    return current_task;
}

/* Task Statistics */
void get_scheduler_stats(scheduler_stats_t *out_stats) {
    if (out_stats) {
        enter_critical();
        memcpy(out_stats, &stats, sizeof(scheduler_stats_t));
        exit_critical();
    }
}

/* Context Switch Handler (PendSV) */
void PendSV_Handler(void) {
    /* Save current context */
    __asm volatile (
        "MRS     R0, PSP          \n"
        "STMDB   R0!, {R4-R11}    \n"
        "LDR     R1, =current_task\n"
        "LDR     R1, [R1]         \n"
        "STR     R0, [R1]         \n"
    );
    
    /* Load next context */
    __asm volatile (
        "LDR     R0, =next_task   \n"
        "LDR     R0, [R0]         \n"
        "LDR     R0, [R0]         \n"
        "LDMIA   R0!, {R4-R11}    \n"
        "MSR     PSP, R0          \n"
        "BX      LR               \n"
    );
}
