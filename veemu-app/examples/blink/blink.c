/**
 * blink.c — Simple GPIO blink
 *
 * Toggles PA5 (LD2) 10 times (5 ON/OFF cycles) then returns.
 * No UART. No SysTick. Pure GPIO.
 *
 * Call from main(): blink_app();
 */
#include "../../platform/stm32f401re.h"
#include "blink.h"

void blink_app(void)
{
    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5 = push-pull output */
    GPIOA->MODER  &= ~(0x3UL << (5 * 2));
    GPIOA->MODER  |=  (0x1UL << (5 * 2));
    GPIOA->OTYPER &= ~(1UL << 5);

    /* Toggle PA5 10 times (5 ON/OFF cycles) */
    for (int i = 0; i < 10; i++) {
        if (i & 1)
            GPIOA->BSRR = (1UL << (5 + 16));  /* OFF */
        else
            GPIOA->BSRR = (1UL << 5);          /* ON  */
    }
}
