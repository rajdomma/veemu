#include "stm32f401re.h"
#include "systick.h"

volatile uint32_t g_tick = 0;

void SysTick_Handler(void)
{
    g_tick++;
}

void systick_init(void)
{
    SYST_RVR = 16000U - 1U;   /* reload: 16MHz / 16000 = 1000Hz → 1ms tick */
    SYST_CVR = 0U;             /* clear current value */
    SYST_CSR = 0x7U;           /* CLKSOURCE=1, TICKINT=1, ENABLE=1 */
}

void delay_ms(uint32_t ms)
{
    uint32_t start = g_tick;
    while ((g_tick - start) < ms) {}
}
