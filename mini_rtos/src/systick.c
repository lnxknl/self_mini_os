#include "stm32f10x.h"
#include "timer.h"
#include "rtos_core.h"

/* System tick counter */
static volatile uint32_t system_ticks = 0;

/* Initialize SysTick for 1ms interrupts */
void systick_init(void) {
    /* Configure SysTick to generate an interrupt every 1ms */
    SysTick_Config(SystemCoreClock / 1000);  /* 1000 Hz = 1ms period */
    
    /* Set SysTick interrupt priority (highest) */
    NVIC_SetPriority(SysTick_IRQn, 0);
}

/* Get current system ticks */
uint32_t get_system_ticks(void) {
    return system_ticks;
}

/* Convert milliseconds to ticks */
uint32_t ms_to_ticks(uint32_t ms) {
    return ms;  /* Since we're running at 1ms per tick */
}

/* SysTick interrupt handler */
void SysTick_Handler(void) {
    /* Increment system tick counter */
    system_ticks++;
    
    /* Get current CPU */
    uint8_t current_cpu = get_current_cpu();
    
    /* Advance timer base for current CPU */
    timer_base_advance(&timer_bases[current_cpu]);
    
    /* Handle any expired timers */
    timer_base_process_events(&timer_bases[current_cpu]);
    
    /* Check if task switch is needed */
    if (system_ticks % TASK_SWITCH_TICKS == 0) {
        schedule_next_task();
    }
}

/* Delay function using SysTick */
void delay_ms(uint32_t ms) {
    uint32_t start = system_ticks;
    while ((system_ticks - start) < ms) {
        /* Wait */
    }
}
