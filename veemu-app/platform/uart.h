#pragma once
#include <stdint.h>

/* ── Simple print API ────────────────────────────────────────────────────
 * Always safe — no FILE, no buffering, no crashes.
 * For formatted output use printf() which also routes through _write.
 *
 * Example:
 *   uart_println("Hello!");
 *   uart_print("tick="); uart_print_u32(g_tick); uart_println("ms");
 *   printf("[MODE] freq=%.2f Hz  count=%d\n", freq, count);
 * ──────────────────────────────────────────────────────────────────────── */

void uart_putc(char c);
void uart_print(const char *s);
void uart_println(const char *s);
void uart_print_u32(uint32_t v);
void uart_print_hex(uint32_t v);
