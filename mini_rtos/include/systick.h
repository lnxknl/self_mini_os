#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/* Number of system ticks between task switches */
#define TASK_SWITCH_TICKS    10  /* Switch tasks every 10ms */

/* Initialize SysTick timer */
void systick_init(void);

/* Get current system ticks */
uint32_t get_system_ticks(void);

/* Convert milliseconds to ticks */
uint32_t ms_to_ticks(uint32_t ms);

/* Delay function */
void delay_ms(uint32_t ms);

#endif /* SYSTICK_H */
