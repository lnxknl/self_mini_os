#include "rtos_core.h"
#include "stm32f10x.h"

/* Global Variables */
static tcb_t *current_task = NULL;
static tcb_t *next_task = NULL;
static tcb_t task_list[MAX_TASKS];
static uint32_t task_count = 0;
static volatile uint32_t tick_count = 0;

/* Memory Management */
static uint8_t heap[HEAP_SIZE];
static mem_block_t *free_list = NULL;

/* Initialize RTOS */
void rtos_init(void) {
    /* Initialize system tick timer */
    SysTick_Config(SYSTICK_PERIOD);
    
    /* Initialize memory management */
    memory_init();
    
    /* Initialize scheduler */
    scheduler_init();
    
    /* Create idle task */
    task_create("idle", idle_task, NULL, IDLE_TASK_PRIORITY, TASK_STACK_SIZE);
}

/* Start RTOS */
void rtos_start(void) {
    /* Enable interrupts */
    __enable_irq();
    
    /* Start scheduler */
    scheduler_start();
    
    /* We should never get here */
    while(1);
}

/* Task Creation */
tcb_t *task_create(const char *name, task_function_t func, void *arg, 
                   uint8_t priority, uint32_t stack_size) {
    tcb_t *task = NULL;
    
    /* Check if we have room for new task */
    if (task_count >= MAX_TASKS) {
        return NULL;
    }
    
    /* Find free TCB */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_list[i].state == TASK_DELETED) {
            task = &task_list[i];
            break;
        }
    }
    
    if (task == NULL) {
        return NULL;
    }
    
    /* Allocate stack */
    task->stack_base = rtos_malloc(stack_size * sizeof(uint32_t));
    if (task->stack_base == NULL) {
        return NULL;
    }
    
    /* Initialize TCB */
    strncpy(task->name, name, 15);
    task->name[15] = '\0';
    task->task_func = func;
    task->arg = arg;
    task->priority = priority;
    task->state = TASK_READY;
    task->stack_size = stack_size;
    
    /* Initialize stack */
    stack_init(task);
    
    /* Add task to scheduler */
    scheduler_add_task(task);
    task_count++;
    
    return task;
}

/* Memory Management */
void memory_init(void) {
    /* Initialize free list with single block */
    free_list = (mem_block_t *)heap;
    free_list->size = HEAP_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
}

void *rtos_malloc(uint32_t size) {
    mem_block_t *block = free_list;
    mem_block_t *prev = NULL;
    
    /* Align size to 4 bytes */
    size = (size + 3) & ~3;
    size += sizeof(mem_block_t);
    
    /* Find suitable block */
    while (block != NULL) {
        if (block->free && block->size >= size) {
            /* Split block if possible */
            if (block->size >= size + sizeof(mem_block_t) + MIN_BLOCK_SIZE) {
                mem_block_t *new_block = (mem_block_t *)((uint8_t *)block + size);
                new_block->size = block->size - size;
                new_block->free = 1;
                new_block->next = block->next;
                
                block->size = size;
                block->next = new_block;
            }
            
            block->free = 0;
            return (void *)((uint8_t *)block + sizeof(mem_block_t));
        }
        
        prev = block;
        block = block->next;
    }
    
    return NULL;
}

/* Context Switching */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "CPSID   I                  \n" /* Disable interrupts */
        "PUSH    {R4-R11}           \n" /* Save remaining registers */
        "LDR     R0, =current_task  \n" /* Get current task */
        "LDR     R1, [R0]           \n"
        "STR     SP, [R1]           \n" /* Save SP to TCB */
        
        "LDR     R1, =next_task     \n" /* Get next task */
        "LDR     R1, [R1]           \n"
        "STR     R1, [R0]           \n" /* Update current task */
        "LDR     SP, [R1]           \n" /* Load SP from TCB */
        
        "POP     {R4-R11}           \n" /* Restore registers */
        "CPSIE   I                  \n" /* Enable interrupts */
        "BX      LR                 \n" /* Return */
    );
}

/* System Tick Handler */
void SysTick_Handler(void) {
    tick_count++;
    scheduler_switch_task();
}

/* Critical Section Management */
void enter_critical(void) {
    __disable_irq();
}

void exit_critical(void) {
    __enable_irq();
}

/* Time Management */
void delay_ms(uint32_t ms) {
    uint32_t start = tick_count;
    while ((tick_count - start) < ms) {
        task_yield();
    }
}

uint32_t get_tick_count(void) {
    return tick_count;
}

/* Idle Task */
static void idle_task(void *arg) {
    while(1) {
        __WFI(); /* Wait for interrupt */
    }
}
