/**
 * main.c — Veemu App Entry Point
 * platform_init() runs before main() via Reset_Handler.
 * UART, SysTick, GPIO are all ready from line 1.
 *
 * Usage:
 *   make APP=blink flash
 *   make APP=led_patterns flash
 *   make APP=tim_test flash
 *   make APP=spi_test flash
 *   make APP=i2c_test flash
 */
#include <stdio.h>

#if defined(APP_BLINK)
  #include "examples/blink/blink.h"
#elif defined(APP_LED_PATTERNS)
  #include "examples/led_patterns/led_patterns.h"
#elif defined(APP_TIM_TEST)
  #include "examples/tim_test/tim_test.h"
#elif defined(APP_SPI_TEST)
  #include "examples/spi_test/spi_test.h"
#elif defined(APP_I2C_TEST)
  #include "examples/i2c_test/i2c_test.h"
#else
  #error "No APP selected. Run: make APP=<name> flash"
#endif

int main(void)
{
    printf("[BOOT] Hello from Veemu vHW!\n");
    printf("[BOOT] platform_init done — UART, SysTick, GPIO ready\n");

#if defined(APP_BLINK)
    blink_app();
#elif defined(APP_LED_PATTERNS)
    ledpatterns_app();
#elif defined(APP_TIM_TEST)
    tim_test();
#elif defined(APP_SPI_TEST)
    spi_test();
#elif defined(APP_I2C_TEST)
    i2c_test();
#endif

    while (1) {}
    return 0;
}