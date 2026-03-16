/**
 * platform_init.c — Board bring-up using generated platform_config.h
 * Called from Reset_Handler before main().
 * Never hardcode register addresses here — all come from JSON via gen_platform.py.
 */
#include <stdint.h>
#include "platform_config.h"

void platform_systick_init(void)
{
    *(volatile uint32_t*)0xE000E014UL = PLATFORM_SYSTICK_LOAD;
    *(volatile uint32_t*)0xE000E018UL = 0;
    *(volatile uint32_t*)0xE000E010UL = 0x07;
}

void platform_uart_init(void)
{
    PLATFORM_RCC_AHB1ENR |= PLATFORM_TX_RCC_AHB1;
#if PLATFORM_UART_RCC_APB1 != 0
    PLATFORM_RCC_APB1ENR |= PLATFORM_UART_RCC_APB1;
    (void)PLATFORM_RCC_APB1ENR;
#endif
#if PLATFORM_UART_RCC_APB2 != 0
    PLATFORM_RCC_APB2ENR |= PLATFORM_UART_RCC_APB2;
    (void)PLATFORM_RCC_APB2ENR;
#endif
    PLATFORM_TX_PORT_MODER &= ~(0x3u << (PLATFORM_TX_PIN * 2u));
    PLATFORM_TX_PORT_MODER |=  (0x2u << (PLATFORM_TX_PIN * 2u));
    if (PLATFORM_TX_PIN < 8u) {
        PLATFORM_TX_PORT_AFR0 &= ~(0xFu << (PLATFORM_TX_PIN * 4u));
        PLATFORM_TX_PORT_AFR0 |=  (PLATFORM_TX_AF << (PLATFORM_TX_PIN * 4u));
    }
    PLATFORM_UART_SR      = 0;
    PLATFORM_UART_CR1     = 0;
    PLATFORM_UART_BRR_REG = PLATFORM_UART_BRR_VAL;
    PLATFORM_UART_CR1     = (1u<<3)|(1u<<13);
}

void platform_gpio_init(void)
{
    PLATFORM_RCC_AHB1ENR |= PLATFORM_LED_RCC_AHB1 | PLATFORM_BTN_RCC_AHB1;
    (void)PLATFORM_RCC_AHB1ENR;

    PLATFORM_LED_PORT_MODER   &= ~(0x3u << (PLATFORM_LED_PIN * 2u));
    PLATFORM_LED_PORT_MODER   |=  (0x1u << (PLATFORM_LED_PIN * 2u));
    PLATFORM_LED_PORT_OTYPER  &= ~(1u   <<  PLATFORM_LED_PIN);
    PLATFORM_LED_PORT_OSPEEDR |=  (0x2u << (PLATFORM_LED_PIN * 2u));
    PLATFORM_LED_PORT_PUPDR   &= ~(0x3u << (PLATFORM_LED_PIN * 2u));
    PLATFORM_LED_PORT_BSRR     =  (1u   << (PLATFORM_LED_PIN + 16u));

    PLATFORM_BTN_PORT_MODER &= ~(0x3u << (PLATFORM_BTN_PIN * 2u));
    PLATFORM_BTN_PORT_PUPDR &= ~(0x3u << (PLATFORM_BTN_PIN * 2u));
    PLATFORM_BTN_PORT_PUPDR |=  (0x1u << (PLATFORM_BTN_PIN * 2u));
}

void platform_init(void)
{
    platform_systick_init();
    platform_uart_init();
    platform_gpio_init();
}

