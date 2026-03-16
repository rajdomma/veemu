/**
 * led_patterns.c — LED Pattern Demo
 *
 * Mode 0: SOS Morse       · · · — — — · · ·
 * Mode 1: Heartbeat ×5    5 rapid pulses + gap
 * Mode 2: Double Flash    two short flashes + gap
 *
 * Uses delay_ms for timing (SysTick-based, proven working).
 * Button checked between every LED step — immediate response.
 * Uses HW API only — no direct register access.
 */

#include "led_patterns.h"
#include "../../platform/gpio.h"
#include "../../platform/uart.h"
#include "../../platform/systick.h"
#include <stdio.h>

static volatile uint32_t g_mode     = 0;
static          uint32_t g_btn_prev = 0;

static const char *MODE_NAMES[] = { "SOS Morse", "Heartbeat x5", "Double Flash" };

/* ── check_button ────────────────────────────────────────────────
 * Non-blocking, call between LED steps.
 * Detects rising edge, changes mode once per press.
 * Returns 1 if mode changed — caller should abort pattern.
 * ---------------------------------------------------------------- */
static int check_button(void)
{
    uint32_t btn = gpio_btn_read();
    int changed = 0;

    if (btn && !g_btn_prev) {
        g_mode = (g_mode + 1u) % 3u;
        gpio_led_off();
        printf("\n[BTN] => mode %lu: %s\n\n",
            (unsigned long)g_mode, MODE_NAMES[g_mode]);
        changed = 1;
    }
    g_btn_prev = btn;
    return changed;
}

/* ── Pattern macros ──────────────────────────────────────────────
 * Each LED step: set LED, wait, check button, abort if changed.
 * delay_ms is used directly — proven working with SysTick.
 * ---------------------------------------------------------------- */
#define LED_ON_MS(ms)  do { gpio_led_on();  delay_ms(ms); if (check_button()) return 1; } while(0)
#define LED_OFF_MS(ms) do { gpio_led_off(); delay_ms(ms); if (check_button()) return 1; } while(0)
#define WAIT_MS(ms)    do {                 delay_ms(ms); if (check_button()) return 1; } while(0)

/* ── Patterns — return 1 if aborted ─────────────────────────────── */
static int pattern_sos(void)
{
    int i;
    for (i=0;i<3;i++) { LED_ON_MS(150); LED_OFF_MS(150); }
    WAIT_MS(200);
    for (i=0;i<3;i++) { LED_ON_MS(400); LED_OFF_MS(150); }
    WAIT_MS(200);
    for (i=0;i<3;i++) { LED_ON_MS(150); LED_OFF_MS(150); }
    WAIT_MS(800);
    return 0;
}

static int pattern_heartbeat(void)
{
    int i;
    for (i=0;i<5;i++) { LED_ON_MS(80); LED_OFF_MS(80); }
    WAIT_MS(600);
    return 0;
}

static int pattern_double_flash(void)
{
    LED_ON_MS(80);  LED_OFF_MS(120);
    LED_ON_MS(80);  LED_OFF_MS(800);
    return 0;
}

/* ── App entry ───────────────────────────────────────────────────── */
void ledpatterns_app(void)
{
    printf("\n========================================\n");
    printf("  STM32F401RE Digital Twin - vHW Test  \n");
    printf("========================================\n");
    printf("  CPU  : ARM Cortex-M4 @ 16MHz HSI\n");
    printf("  Flash: 512KB @ 0x08000000\n");
    printf("  SRAM : 96KB  @ 0x20000000\n");
    printf("  UART : USART2 PA2 @ 115200\n");
    printf("========================================\n\n");
    printf("[INFO] Press B1 to cycle LED patterns\n\n");
    printf("[MODE] Active: %s\n", MODE_NAMES[g_mode]);

    uint32_t loop = 0;

    while (1) {
        loop++;

        if (loop % 5 == 1)
            printf("[LOOP #%03lu] mode=%lu (%s) tick=%lums  LED=%lu  BTN=%lu\n",
                (unsigned long)loop,
                (unsigned long)g_mode,
                MODE_NAMES[g_mode],
                (unsigned long)g_tick,
                (unsigned long)gpio_led_get(),
                (unsigned long)gpio_btn_read());

        int aborted = 0;
        switch (g_mode) {
            case 0: aborted = pattern_sos();          break;
            case 1: aborted = pattern_heartbeat();    break;
            case 2: aborted = pattern_double_flash(); break;
        }

        if (aborted) {
            printf("[MODE] Active: %s\n", MODE_NAMES[g_mode]);
        }
    }
}