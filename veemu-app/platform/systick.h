#pragma once
#include <stdint.h>

extern volatile uint32_t g_tick;

void systick_init(void);
void delay_ms(uint32_t ms);
