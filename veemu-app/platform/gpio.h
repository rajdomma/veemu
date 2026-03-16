#pragma once
#include <stdint.h>

/* ── LED (PA5, Active HIGH) ──────────────────────────────── */
void     gpio_led_on(void);
void     gpio_led_off(void);
void     gpio_led_toggle(void);
uint32_t gpio_led_get(void);        /* 1 = ON, 0 = OFF */

/* ── Button (PC13, Active LOW, pull-up) ──────────────────── */
uint32_t gpio_btn_read(void);       /* 1 = pressed, 0 = not pressed */