#include "rtos_core.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/* LED Tasks */
static void led1_task(void *arg) {
    /* Configure LED1 pin (PA1) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef gpio_init;
    gpio_init.GPIO_Pin = GPIO_Pin_1;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_init);
    
    while(1) {
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
        delay_ms(500);
        GPIO_ResetBits(GPIOA, GPIO_Pin_1);
        delay_ms(500);
    }
}

static void led2_task(void *arg) {
    /* Configure LED2 pin (PA2) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef gpio_init;
    gpio_init.GPIO_Pin = GPIO_Pin_2;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_init);
    
    while(1) {
        GPIO_SetBits(GPIOA, GPIO_Pin_2);
        delay_ms(300);
        GPIO_ResetBits(GPIOA, GPIO_Pin_2);
        delay_ms(300);
    }
}

/* Main Function */
int main(void) {
    /* Initialize RTOS */
    rtos_init();
    
    /* Create tasks */
    task_create("LED1", led1_task, NULL, 1, TASK_STACK_SIZE);
    task_create("LED2", led2_task, NULL, 1, TASK_STACK_SIZE);
    
    /* Start RTOS */
    rtos_start();
    
    /* We should never get here */
    while(1);
    
    return 0;
}
