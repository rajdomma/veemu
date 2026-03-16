#include "gpio.h"
#include "platform_config.h"

void gpio_led_on(void)
{
    PLATFORM_LED_PORT_BSRR = (1u << PLATFORM_LED_PIN);
}

void gpio_led_off(void)
{
    PLATFORM_LED_PORT_BSRR = (1u << (PLATFORM_LED_PIN + 16u));
}

void gpio_led_toggle(void)
{
    /* Read ODR, flip LED bit, write back via BSRR */
    uint32_t odr = *(volatile uint32_t *)(PLATFORM_LED_PORT_BASE + 0x14u);
    if (odr & (1u << PLATFORM_LED_PIN))
        gpio_led_off();
    else
        gpio_led_on();
}

uint32_t gpio_led_get(void)
{
    uint32_t odr = *(volatile uint32_t *)(PLATFORM_LED_PORT_BASE + 0x14u);
    return (odr >> PLATFORM_LED_PIN) & 1u;
}

uint32_t gpio_btn_read(void)
{
    /* PC13 active LOW — invert so 1 = pressed */
    uint32_t idr = *(volatile uint32_t *)(PLATFORM_BTN_PORT_BASE + 0x10u);
    return !((idr >> PLATFORM_BTN_PIN) & 1u);
}