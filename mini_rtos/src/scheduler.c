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

/* Scheduler Configuration */
static sched_policy_t current_policy = SCHED_PRIORITY;
static uint8_t preemption_mode = PREEMPT_ENABLE;
static uint32_t default_time_slice = 10;  /* 10ms default time slice */

/* EDF Queue */
static tcb_t *edf_queue = NULL;

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

/* Add Task to Ready Queue */
void scheduler_add_task(tcb_t *task) {
    if (!task) return;
    
    enter_critical();
    
    uint8_t priority = task->priority;
    
    /* Add to priority queue */
    task->next = ready_queue[priority];
    if (ready_queue[priority]) {
        ready_queue[priority]->prev = task;
    }
    ready_queue[priority] = task;
    task->prev = NULL;
    
    /* Update ready groups bitmap */
    ready_groups |= (1 << (priority >> 3));
    ready_table[priority >> 3] |= (1 << (priority & 0x07));
    
    exit_critical();
}

/* Remove Task from Ready Queue */
void scheduler_remove_task(tcb_t *task) {
    if (!task) return;
    
    enter_critical();
    
    uint8_t priority = task->priority;
    
    /* Remove from priority queue */
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        ready_queue[priority] = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    }
    
    /* Update ready groups if queue is empty */
    if (ready_queue[priority] == NULL) {
        ready_table[priority >> 3] &= ~(1 << (priority & 0x07));
        if (ready_table[priority >> 3] == 0) {
            ready_groups &= ~(1 << (priority >> 3));
        }
    }
    
    task->next = task->prev = NULL;
    
    exit_critical();
}

/* Get Highest Priority Task */
static tcb_t *get_highest_priority_task(void) {
    if (ready_groups == 0) {
        return idle_task;  /* No tasks ready, return idle task */
    }
    
    /* Find highest priority group using CLZ (Count Leading Zeros) */
    uint32_t group = 31 - __CLZ(ready_groups);
    
    /* Find highest priority task in group */
    uint32_t priority_mask = ready_table[group];
    uint32_t bit_pos = 31 - __CLZ(priority_mask);
    uint32_t priority = (group << 3) + bit_pos;
    
    /* Return first task in that priority queue */
    return ready_queue[priority] ? ready_queue[priority] : idle_task;
}

/* Get Next Task Based on Current Policy */
static tcb_t *get_next_task(void) {
    switch (current_policy) {
        case SCHED_RR:
            return get_next_rr_task();
        case SCHED_EDF:
            return get_next_edf_task();
        case SCHED_HYBRID:
            return get_next_hybrid_task();
        case SCHED_PRIORITY:
        default:
            return get_highest_priority_task();
    }
}

/* Round Robin Scheduling */
static tcb_t *get_next_rr_task(void) {
    if (current_task && current_task->ticks_remaining > 0) {
        return current_task;  /* Continue with current task */
    }
    
    /* Find next ready task at same priority */
    uint8_t priority = current_task ? current_task->priority : 0;
    tcb_t *task = ready_queue[priority];
    
    if (task) {
        /* Move to end of queue */
        ready_queue[priority] = task->next;
        if (task->next) {
            task->next->prev = NULL;
        }
        
        /* Reset time slice */
        task->ticks_remaining = task->time_slice;
        return task;
    }
    
    /* No tasks at this priority, fall back to priority scheduling */
    return get_highest_priority_task();
}

/* EDF Scheduling */
static tcb_t *get_next_edf_task(void) {
    tcb_t *best_task = NULL;
    uint32_t earliest_deadline = UINT32_MAX;
    
    /* Find task with earliest deadline */
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        tcb_t *task = ready_queue[i];
        while (task) {
            if (task->flags & TASK_FLAG_DEADLINE) {
                if (task->deadline < earliest_deadline) {
                    earliest_deadline = task->deadline;
                    best_task = task;
                }
            }
            task = task->next;
        }
    }
    
    return best_task ? best_task : idle_task;
}

/* Hybrid Scheduling (Priority + RR) */
static tcb_t *get_next_hybrid_task(void) {
    tcb_t *highest = get_highest_priority_task();
    
    /* If multiple tasks at highest priority, use RR */
    if (highest && highest->next && 
        highest->next->priority == highest->priority) {
        return get_next_rr_task();
    }
    
    return highest;
}

/* Priority Inheritance */
void inherit_priority(tcb_t *blocker, tcb_t *blocked) {
    if (!blocker || !blocked || 
        blocker->priority >= blocked->priority) {
        return;
    }
    
    enter_critical();
    
    /* Remove blocker from its current queue */
    scheduler_remove_task(blocker);
    
    /* Save original priority if not already inherited */
    if (!(blocker->flags & TASK_FLAG_INHERITED)) {
        blocker->original_priority = blocker->priority;
        blocker->flags |= TASK_FLAG_INHERITED;
    }
    
    /* Inherit priority */
    blocker->priority = blocked->priority;
    blocker->inheritance_depth++;
    
    /* Update blocking chain */
    blocked->blocked_by = blocker;
    blocker->blocking = blocked;
    
    /* Re-add to ready queue with new priority */
    scheduler_add_task(blocker);
    
    stats.priority_inversions++;
    
    exit_critical();
}

void release_inherited_priority(tcb_t *task) {
    if (!task || !(task->flags & TASK_FLAG_INHERITED)) {
        return;
    }
    
    enter_critical();
    
    /* Remove from current queue */
    scheduler_remove_task(task);
    
    /* Restore original priority */
    task->priority = task->original_priority;
    task->flags &= ~TASK_FLAG_INHERITED;
    task->inheritance_depth = 0;
    
    /* Clear blocking chain */
    if (task->blocking) {
        task->blocking->blocked_by = NULL;
        task->blocking = NULL;
    }
    
    /* Re-add to ready queue with original priority */
    scheduler_add_task(task);
    
    exit_critical();
}

/* Preemption Control */
void set_preemption_mode(uint8_t mode) {
    enter_critical();
    preemption_mode = mode;
    exit_critical();
}

void disable_preemption(void) {
    enter_critical();
    preemption_mode = PREEMPT_DISABLE;
    exit_critical();
}

void enable_preemption(void) {
    enter_critical();
    preemption_mode = PREEMPT_ENABLE;
    exit_critical();
}

/* Enhanced Schedule Next Task */
void schedule_next_task(void) {
    enter_critical();
    
    /* Update statistics */
    if (current_task) {
        current_task->run_time += (get_system_ticks() - current_task->last_run);
        
        /* Check for deadline miss */
        if ((current_task->flags & TASK_FLAG_DEADLINE) &&
            get_system_ticks() > current_task->deadline) {
            current_task->deadline_misses++;
            stats.deadline_misses++;
        }
    }
    
    /* Process blocked tasks */
    process_blocked_tasks();
    
    /* Get next task based on policy */
    next_task = get_next_task();
    
    if (next_task != current_task) {
        /* Check preemption */
        if (preemption_mode == PREEMPT_DISABLE ||
            (preemption_mode == PREEMPT_COND && 
             current_task && 
             (current_task->flags & TASK_FLAG_CRITICAL))) {
            next_task = current_task;
            exit_critical();
            return;
        }
        
        /* Update task states */
        if (current_task) {
            if (current_task->state == TASK_RUNNING) {
                current_task->state = TASK_READY;
                current_task->num_preemptions++;
                stats.preemptions++;
                scheduler_add_task(current_task);
            }
        }
        
        next_task->state = TASK_RUNNING;
        next_task->last_run = get_system_ticks();
        next_task->num_activations++;
        scheduler_remove_task(next_task);
        
        stats.context_switches++;
        
        /* Trigger context switch */
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
    
    exit_critical();
}

/* Start Scheduler */
void scheduler_start(void) {
    if (task_count == 0) {
        return;  /* No tasks to run */
    }
    
    /* Get first task to run */
    next_task = get_next_task();
    current_task = next_task;
    
    /* Initialize PSP for first task */
    __set_PSP((uint32_t)next_task->stack_ptr);
    
    /* Switch to PSP */
    __set_CONTROL(0x02);
    __ISB();
    
    /* Enable PendSV interrupt */
    NVIC_SetPriority(PendSV_IRQn, 0xFF);  /* Lowest priority */
    
    /* Start first task */
    __asm volatile (
        "MOV     R0, %0           \n"
        "LDMIA   R0!, {R4-R11}    \n"
        "MSR     PSP, R0          \n"
        "MOV     LR, #0xFFFFFFFD  \n" /* Exception return using PSP */
        "BX      LR               \n"
        :: "r"(next_task->stack_ptr)
    );
}

/* Task Yield */
void task_yield(void) {
    enter_critical();
    
    if (current_task) {
        /* Move current task to end of its priority queue */
        current_task->ticks_remaining = current_task->time_slice;
        current_task->state = TASK_READY;
        scheduler_add_task(current_task);
    }
    
    exit_critical();
    
    /* Schedule next task */
    schedule_next_task();
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
