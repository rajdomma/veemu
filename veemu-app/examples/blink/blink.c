/**
 * blink.c — Simple LED Blink
 *
 * Blinks PA5 (LD2) continuously at 1Hz (500ms ON, 500ms OFF).
 * Uses platform HW API — no direct register access.
 *
 * Call from main(): blink_app();
 */

#include "blink.h"
#include "../../platform/gpio.h"
#include "../../platform/systick.h"
#include <stdio.h>

void blink_app(void)
{
    printf("[BLINK] Starting — PA5 blink 1Hz\n");

    uint32_t count = 0;

    while (1) {
        gpio_led_on();
        delay_ms(500);

        gpio_led_off();
        delay_ms(500);

        count++;
        if (count % 10 == 0)
            printf("[BLINK] %lu blinks, tick=%lums\n",
                (unsigned long)count,
                (unsigned long)g_tick);
    }
}
