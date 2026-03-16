/**
 * led_patterns.c — LED Pattern Demo
 *
 * Mode 0: SOS Morse       · · · — — — · · ·
 * Mode 1: Heartbeat ×5    5 rapid pulses + gap
 * Mode 2: Double Flash    two short flashes + gap
 *
 * Uses HW API only — no direct register access.
 * Button checked once per pattern cycle (simple, matches old working logic).
 */

#include "led_patterns.h"
#include "../../platform/gpio.h"
#include "../../platform/uart.h"
#include "../../platform/systick.h"
#include <stdio.h>

static void pattern_sos(void)
{
    int i;
    for (i=0;i<3;i++){gpio_led_on();delay_ms(150);gpio_led_off();delay_ms(150);}
    delay_ms(200);
    for (i=0;i<3;i++){gpio_led_on();delay_ms(400);gpio_led_off();delay_ms(150);}
    delay_ms(200);
    for (i=0;i<3;i++){gpio_led_on();delay_ms(150);gpio_led_off();delay_ms(150);}
    delay_ms(800);
}

static void pattern_heartbeat(void)
{
    int i;
    for (i=0;i<5;i++){gpio_led_on();delay_ms(80);gpio_led_off();delay_ms(80);}
    delay_ms(600);
}

static void pattern_double_flash(void)
{
    gpio_led_on(); delay_ms(80);
    gpio_led_off();delay_ms(120);
    gpio_led_on(); delay_ms(80);
    gpio_led_off();delay_ms(800);
}

static void confirm_blink(void)
{
    int i;
    for (i=0;i<3;i++){gpio_led_on();delay_ms(50);gpio_led_off();delay_ms(50);}
}

void ledpatterns_app(void)
{
    const char *modes[] = { "SOS Morse", "Heartbeat x5", "Double Flash" };
    uint32_t mode = 0, btn_prev = 0, loop = 0;

    printf("\n========================================\n");
    printf("  STM32F401RE Digital Twin - vHW Test  \n");
    printf("========================================\n");
    printf("  CPU  : ARM Cortex-M4 @ 16MHz HSI\n");
    printf("  Flash: 512KB @ 0x08000000\n");
    printf("  SRAM : 96KB  @ 0x20000000\n");
    printf("  UART : USART2 PA2 @ 115200\n");
    printf("========================================\n\n");
    printf("[INFO] Press B1 to cycle LED patterns\n\n");
    printf("[MODE] Active: %s\n", modes[mode]);

    while (1) {
        loop++;

        if (loop % 5 == 1)
            printf("[LOOP #%03lu] mode=%lu (%s) tick=%lums  LED=%lu  BTN=%lu\n",
                (unsigned long)loop,
                (unsigned long)mode,
                modes[mode],
                (unsigned long)g_tick,
                (unsigned long)gpio_led_get(),
                (unsigned long)gpio_btn_read());

        switch (mode) {
            case 0: pattern_sos();          break;
            case 1: pattern_heartbeat();    break;
            case 2: pattern_double_flash(); break;
        }

        uint32_t btn = gpio_btn_read();
        if (btn && !btn_prev) {
            delay_ms(50);
            if (gpio_btn_read()) {
                mode = (mode + 1U) % 3U;
                printf("\n[BTN] => mode %lu: %s\n\n",
                    (unsigned long)mode, modes[mode]);
                confirm_blink();
            }
        }
        btn_prev = btn;
    }
}
