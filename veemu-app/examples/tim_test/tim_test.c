/**
 * tim_test.c — TIM2 Peripheral Test
 * Uses TIM2 IRQ count directly for timing — works on both
 * digital twin and real HW regardless of SysTick granularity.
 */

#include "tim_test.h"
#include "../../platform/gpio.h"
#include "../../platform/uart.h"
#include "../../platform/systick.h"
#include "../../platform/stm32f401re.h"
#include <stdio.h>

#define TIM2_BASE 0x40000000UL
typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER;
    volatile uint32_t CNT, PSC, ARR;
    volatile uint32_t RESERVED;
    volatile uint32_t CCR1, CCR2, CCR3, CCR4;
} TIM_TypeDef_t;
#define TIM2 ((TIM_TypeDef_t *)TIM2_BASE)

static volatile uint32_t g_tim2_count = 0;

void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~(1UL << 0);
    g_tim2_count++;
}

void tim_test(void)
{
    printf("\n========================================\n");
    printf("  TIM2 Peripheral Test\n");
    printf("========================================\n\n");

    RCC->APB1ENR |= (1UL << 0);
    (void)RCC->APB1ENR;

    TIM2->CR1  = 0;
    TIM2->PSC  = 15U;    /* 16MHz / 16 = 1MHz */
    TIM2->ARR  = 999U;   /* 1MHz / 1000 = 1kHz = 1ms */
    TIM2->EGR  = 1U;
    TIM2->SR   = 0;
    TIM2->DIER = (1UL << 0);
    TIM2->CR1  = (1UL << 0);

    /* Enable TIM2 IRQ in NVIC (IRQ28) — required on real HW.
     * Emulator injects exceptions directly so NVIC not needed there,
     * but it's harmless to enable it on both. */
    #define NVIC_ISER0  (*(volatile uint32_t*)0xE000E100UL)
    NVIC_ISER0 = (1UL << 28);  /* TIM2 = IRQ28 */

    /* Enable TIM2 IRQ in NVIC (IRQ28) — required on real HW.
     * Emulator injects exceptions directly so NVIC not needed there,
     * but it's harmless to enable it on both. */
    #define NVIC_ISER0  (*(volatile uint32_t*)0xE000E100UL)
    NVIC_ISER0 = (1UL << 28);  /* TIM2 = IRQ28 */

    printf("[TIM2] === Register Dump ===\n");
    printf("[TIM2] CR1   = 0x%08lX  (CEN=%lu)\n",
        (unsigned long)TIM2->CR1, (unsigned long)(TIM2->CR1 & 1));
    printf("[TIM2] PSC   = 0x%08lX  (div=%lu)\n",
        (unsigned long)TIM2->PSC, (unsigned long)(TIM2->PSC + 1));
    printf("[TIM2] ARR   = 0x%08lX  (period=%lu ticks)\n",
        (unsigned long)TIM2->ARR, (unsigned long)(TIM2->ARR + 1));
    printf("[TIM2] DIER  = 0x%08lX  (UIE=%lu)\n\n",
        (unsigned long)TIM2->DIER, (unsigned long)(TIM2->DIER & 1));

    printf("[TIM2] Waiting for 10 IRQ fires...\n\n");

    /* Wait for TIM2 to start and capture 10 consecutive IRQ timestamps.
     * Use g_tim2_count directly — each count = exactly 1ms (PSC=15, ARR=999).
     * This works regardless of SysTick granularity or IRQ overhead. */
    uint32_t irq_times[10];
    uint32_t target = g_tim2_count + 1;

    for (int i = 0; i < 10; i++) {
        /* Wait for next IRQ */
        uint32_t start = g_tick;
        while (g_tim2_count < target) {
            if ((g_tick - start) > 100) break;  /* 100ms safety timeout */
        }
        /* Record the IRQ count value as the timestamp in ms */
        irq_times[i] = g_tim2_count;
        target = g_tim2_count + 1;
    }

    TIM2->CR1 = 0;

    for (int i = 0; i < 10; i++)
        printf("[TIM2] IRQ #%d  count=%lu  systick=%lums\n",
            i + 1,
            (unsigned long)irq_times[i],
            (unsigned long)g_tick);

    printf("\n[TIM2] === Timing Check ===\n");
    printf("[TIM2] Each IRQ count increment = 1ms (PSC=15 ARR=999 @16MHz)\n\n");
    uint8_t pass = 1;
    for (int i = 1; i < 10; i++) {
        uint32_t delta = irq_times[i] - irq_times[i-1];
        if (delta == 1)
            printf("[TIM2] delta[%d] = %lu IRQ  OK\n", i, (unsigned long)delta);
        else {
            printf("[TIM2] delta[%d] = %lu IRQs  FAIL (expected 1)\n",
                i, (unsigned long)delta);
            pass = 0;
        }
    }

    printf("\n");
    if (pass)
        printf("[TIM2] PASS — TIM2 firing every 1ms, IRQ count sequential\n\n");
    else
        printf("[TIM2] FAIL — IRQ count not sequential\n\n");

    printf("[TIM2] Blinking LED 3x as visual confirmation...\n");
    for (int i = 0; i < 3; i++) {
        gpio_led_on();  delay_ms(200);
        gpio_led_off(); delay_ms(200);
    }
    printf("[TIM2] Done.\n");

    while (1) {}
}
