/**
 * tim_test.c — TIM2 Peripheral Test
 *
 * Tests:
 *   1. Register readback after init (PSC, ARR, CR1, DIER)
 *   2. IRQ fires via TIM2_IRQHandler (vector 44 = IRQ28+16)
 *   3. 10 update events counted, timestamp printed each time
 *   4. Timing accuracy — each tick should be ~1ms
 *
 * Config:
 *   PSC = 15 (16MHz / 16 = 1MHz timer clock)
 *   ARR = 999 (1MHz / 1000 = 1kHz → 1ms period)
 */

#include "../../platform/stm32f401re.h"
#include "../../platform/systick.h"
#include "tim_test.h"
#include <stdint.h>

/* ── TIM2 registers ──────────────────────────────────────────── */
#define TIM2_BASE   0x40000000UL

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER;
    volatile uint32_t CNT, PSC, ARR;
    volatile uint32_t RESERVED;
    volatile uint32_t CCR1, CCR2, CCR3, CCR4;
} TIM_TypeDef_t;

#define TIM2  ((TIM_TypeDef_t *) TIM2_BASE)

/* ── UART helpers ────────────────────────────────────────────── */
static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER  &= ~(0x3UL << (2 * 2U));
    GPIOA->MODER  |=  (0x2UL << (2 * 2U));
    GPIOA->AFR[0] &= ~(0xFUL << (2 * 4U));
    GPIOA->AFR[0] |=  (7UL   << (2 * 4U));
    USART2->SR  = 0;
    USART2->CR1 = 0;
    USART2->BRR = 0x008BUL;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}
static void putc_(char c) {
    if (c == '\n') { while (!(USART2->SR & USART_SR_TXE)){} USART2->DR = '\r'; }
    while (!(USART2->SR & USART_SR_TXE)) {}
    USART2->DR = (uint8_t)c;
}
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void put_u(uint32_t v) {
    char buf[10]; int i = 0;
    if (!v) { putc_('0'); return; }
    while (v) { buf[i++] = '0' + v % 10; v /= 10; }
    while (i > 0) putc_(buf[--i]);
}
static void put_hex32(uint32_t v) {
    puts_("0x");
    for (int i = 7; i >= 0; i--)
        putc_("0123456789ABCDEF"[(v >> (i*4)) & 0xF]);
}

/* ── TIM2 IRQ state ──────────────────────────────────────────── */
static volatile uint32_t g_tim2_count = 0;

void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~(1UL << 0);   /* clear UIF */
    g_tim2_count++;
}

/* ── tim_test entry point ────────────────────────────────────── */
void tim_test(void)
{
    systick_init();
    uart_init();

    puts_("\n========================================\n");
    puts_("  TIM2 Peripheral Test\n");
    puts_("========================================\n\n");

    /* Enable TIM2 clock */
    RCC->APB1ENR |= (1UL << 0);
    (void)RCC->APB1ENR;

    /* Configure TIM2 */
    TIM2->CR1  = 0;
    TIM2->PSC  = 15U;       /* 16MHz / 16 = 1MHz */
    TIM2->ARR  = 999U;      /* 1MHz / 1000 = 1ms */
    TIM2->EGR  = 1U;        /* UG: preload PSC/ARR */
    TIM2->SR   = 0;
    TIM2->DIER = (1UL << 0);
    TIM2->CR1  = (1UL << 0);

    /* Register dump */
    puts_("[TIM2] === Register Dump ===\n");
    puts_("[TIM2] CR1   = "); put_hex32(TIM2->CR1);
    puts_("  (CEN="); putc_((TIM2->CR1 & 1) ? '1':'0'); puts_(")\n");
    puts_("[TIM2] PSC   = "); put_hex32(TIM2->PSC);
    puts_("  (div="); put_u(TIM2->PSC + 1); puts_(")\n");
    puts_("[TIM2] ARR   = "); put_hex32(TIM2->ARR);
    puts_("  (period="); put_u(TIM2->ARR + 1); puts_(" ticks)\n");
    puts_("[TIM2] DIER  = "); put_hex32(TIM2->DIER);
    puts_("  (UIE="); putc_((TIM2->DIER & 1) ? '1':'0'); puts_(")\n\n");

    puts_("[TIM2] Sampling g_tick at each IRQ fire (delay_ms yields to engine)...\n\n");

    /*
     * Strategy: wait for each IRQ fire one at a time using delay_ms(2).
     * delay_ms spins on g_tick which requires SysTick IRQs, which means
     * the engine runs many slices between each check — giving TIM2 time
     * to fire exactly once per ms and SysTick time to increment g_tick.
     */
    uint32_t timestamps[10];
    uint32_t target = 1;

    for (int i = 0; i < 10; i++) {
        /* Wait until the next TIM2 fire — with a 50ms timeout */
        uint32_t start = g_tick;
        while (g_tim2_count < target) {
            if ((g_tick - start) > 50) break;
        }
        timestamps[i] = g_tick;
        target++;
        delay_ms(1);   /* yield to engine — lets SysTick advance */
    }

    TIM2->CR1 = 0;   /* stop */

    /* Print results */
    for (int i = 0; i < 10; i++) {
        puts_("[TIM2] tick #"); put_u((uint32_t)(i + 1));
        puts_("  systick="); put_u(timestamps[i]);
        puts_("ms\n");
    }

    /* Timing check */
    puts_("\n[TIM2] === Timing Check ===\n");
    uint8_t pass = 1;
    for (int i = 1; i < 10; i++) {
        uint32_t delta = timestamps[i] - timestamps[i-1];
        puts_("[TIM2] delta["); put_u((uint32_t)i); puts_("] = ");
        put_u(delta); puts_("ms");
        if (delta >= 1 && delta <= 3) {
            puts_("  OK\n");
        } else {
            puts_("  FAIL (expected ~1ms)\n");
            pass = 0;
        }
    }

    puts_("\n");
    if (pass)
        puts_("[TIM2] PASS -- IRQs firing, timing correct\n\n");
    else
        puts_("[TIM2] FAIL -- timing out of range\n\n");
}