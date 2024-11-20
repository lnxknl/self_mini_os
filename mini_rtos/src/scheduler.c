#include "rtos_core.h"
#include "stm32f10x.h"

/* Priority Queue for Ready Tasks */
static tcb_t *ready_queue[MAX_PRIORITY + 1];
static uint32_t ready_groups = 0;
static uint32_t ready_table[32];

/* Initialize Scheduler */
void scheduler_init(void) {
    /* Clear ready queue */
    for (int i = 0; i <= MAX_PRIORITY; i++) {
        ready_queue[i] = NULL;
    }
    
    /* Initialize ready table */
    for (int i = 0; i < 32; i++) {
        ready_table[i] = 0;
    }
    ready_groups = 0;
}

/* Start Scheduler */
void scheduler_start(void) {
    /* Get highest priority task */
    next_task = get_highest_priority_task();
    if (next_task == NULL) {
        return;
    }
    
    /* Set current task */
    current_task = next_task;
    
    /* Load first task context */
    __asm volatile (
        "LDR     R0, =next_task     \n"
        "LDR     R0, [R0]           \n"
        "LDR     SP, [R0]           \n"
        "POP     {R4-R11}           \n"
        "POP     {R0-R3}            \n"
        "POP     {R12}              \n"
        "POP     {LR}               \n"
        "POP     {PC}               \n"
    );
}

/* Add Task to Ready Queue */
void scheduler_add_task(tcb_t *task) {
    uint8_t priority = task->priority;
    
    /* Add to priority queue */
    task->next = ready_queue[priority];
    if (ready_queue[priority] != NULL) {
        ready_queue[priority]->prev = task;
    }
    ready_queue[priority] = task;
    
    /* Update ready groups */
    ready_groups |= (1 << (priority >> 3));
    ready_table[priority >> 3] |= (1 << (priority & 0x07));
}

/* Remove Task from Ready Queue */
void scheduler_remove_task(tcb_t *task) {
    uint8_t priority = task->priority;
    
    /* Remove from priority queue */
    if (task->prev != NULL) {
        task->prev->next = task->next;
    } else {
        ready_queue[priority] = task->next;
    }
    
    if (task->next != NULL) {
        task->next->prev = task->prev;
    }
    
    /* Update ready groups if queue is empty */
    if (ready_queue[priority] == NULL) {
        ready_table[priority >> 3] &= ~(1 << (priority & 0x07));
        if (ready_table[priority >> 3] == 0) {
            ready_groups &= ~(1 << (priority >> 3));
        }
    }
}

/* Get Highest Priority Task */
static tcb_t *get_highest_priority_task(void) {
    if (ready_groups == 0) {
        return NULL;
    }
    
    /* Find highest priority group */
    uint32_t group = __CLZ(__RBIT(ready_groups));
    uint32_t priority = group << 3;
    
    /* Find highest priority task in group */
    uint32_t priority_mask = ready_table[group];
    priority += __CLZ(__RBIT(priority_mask));
    
    return ready_queue[priority];
}

/* Switch Task */
void scheduler_switch_task(void) {
    /* Get highest priority task */
    next_task = get_highest_priority_task();
    if (next_task == NULL || next_task == current_task) {
        return;
    }
    
    /* Trigger context switch */
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

/* Task Yield */
void task_yield(void) {
    /* Move current task to end of its priority queue */
    if (current_task != NULL) {
        uint8_t priority = current_task->priority;
        scheduler_remove_task(current_task);
        scheduler_add_task(current_task);
    }
    
    /* Switch task */
    scheduler_switch_task();
}
