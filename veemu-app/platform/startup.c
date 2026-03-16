/* startup.c — STM32F401RE minimal startup */
#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);
extern void platform_init(void);

/* ── Handler declarations ───────────────────────────────────── */
void Reset_Handler(void);
void Default_Handler(void);

/* Core exceptions */
void NMI_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)__attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)__attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)__attribute__((weak,alias("Default_Handler")));
void SVC_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)  __attribute__((weak, alias("Default_Handler")));

/* STM32F401RE peripheral IRQs (IRQ0–IRQ85) */
void WWDG_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void PVD_IRQHandler(void)               __attribute__((weak, alias("Default_Handler")));
void TAMP_STAMP_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void RTC_WKUP_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void FLASH_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void RCC_IRQHandler(void)               __attribute__((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream2_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream3_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream4_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream5_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream6_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void ADC_IRQHandler(void)               __attribute__((weak, alias("Default_Handler")));
void EXTI9_5_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM1_BRK_TIM9_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM1_UP_TIM10_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM1_TRG_COM_TIM11_IRQHandler(void)__attribute__((weak, alias("Default_Handler")));
void TIM1_CC_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM2_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void TIM3_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void TIM4_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void SPI2_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void EXTI15_10_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void RTC_Alarm_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void OTG_FS_WKUP_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream7_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void SDIO_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void TIM5_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void SPI3_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream0_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream1_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream2_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream3_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream4_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void OTG_FS_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream5_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream6_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream7_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void USART6_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void I2C3_EV_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void I2C3_ER_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void FPU_IRQHandler(void)               __attribute__((weak, alias("Default_Handler")));
void SPI4_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));

/* ── Vector table ───────────────────────────────────────────── */
/* Total: 16 core + 86 IRQs = 102 entries                        */
__attribute__((section(".isr_vector")))
uint32_t const vector_table[] = {
    /* Core (0–15) */
    (uint32_t)&_estack,                      /*  0: Initial SP */
    (uint32_t)Reset_Handler,                 /*  1: Reset */
    (uint32_t)NMI_Handler,                   /*  2: NMI */
    (uint32_t)HardFault_Handler,             /*  3: HardFault */
    (uint32_t)MemManage_Handler,             /*  4: MemManage */
    (uint32_t)BusFault_Handler,              /*  5: BusFault */
    (uint32_t)UsageFault_Handler,            /*  6: UsageFault */
    0, 0, 0, 0,                              /*  7-10: reserved */
    (uint32_t)SVC_Handler,                   /* 11: SVCall */
    (uint32_t)DebugMon_Handler,              /* 12: DebugMonitor */
    0,                                       /* 13: reserved */
    (uint32_t)PendSV_Handler,                /* 14: PendSV */
    (uint32_t)SysTick_Handler,               /* 15: SysTick */

    /* IRQ0–IRQ18 */
    (uint32_t)WWDG_IRQHandler,              /* 16: IRQ0  WWDG */
    (uint32_t)PVD_IRQHandler,               /* 17: IRQ1  PVD */
    (uint32_t)TAMP_STAMP_IRQHandler,        /* 18: IRQ2  TAMP/STAMP */
    (uint32_t)RTC_WKUP_IRQHandler,          /* 19: IRQ3  RTC wakeup */
    (uint32_t)FLASH_IRQHandler,             /* 20: IRQ4  Flash */
    (uint32_t)RCC_IRQHandler,               /* 21: IRQ5  RCC */
    (uint32_t)EXTI0_IRQHandler,             /* 22: IRQ6  EXTI0 */
    (uint32_t)EXTI1_IRQHandler,             /* 23: IRQ7  EXTI1 */
    (uint32_t)EXTI2_IRQHandler,             /* 24: IRQ8  EXTI2 */
    (uint32_t)EXTI3_IRQHandler,             /* 25: IRQ9  EXTI3 */
    (uint32_t)EXTI4_IRQHandler,             /* 26: IRQ10 EXTI4 */
    (uint32_t)DMA1_Stream0_IRQHandler,      /* 27: IRQ11 DMA1 S0 */
    (uint32_t)DMA1_Stream1_IRQHandler,      /* 28: IRQ12 DMA1 S1 */
    (uint32_t)DMA1_Stream2_IRQHandler,      /* 29: IRQ13 DMA1 S2 */
    (uint32_t)DMA1_Stream3_IRQHandler,      /* 30: IRQ14 DMA1 S3 */
    (uint32_t)DMA1_Stream4_IRQHandler,      /* 31: IRQ15 DMA1 S4 */
    (uint32_t)DMA1_Stream5_IRQHandler,      /* 32: IRQ16 DMA1 S5 */
    (uint32_t)DMA1_Stream6_IRQHandler,      /* 33: IRQ17 DMA1 S6 */
    (uint32_t)ADC_IRQHandler,               /* 34: IRQ18 ADC */

    /* IRQ19–IRQ23: reserved on F401 */
    0, 0, 0, 0,                             /* 35-38: IRQ19-22 reserved */

    /* IRQ23–IRQ27 */
    (uint32_t)EXTI9_5_IRQHandler,           /* 39: IRQ23 EXTI9_5 */
    (uint32_t)TIM1_BRK_TIM9_IRQHandler,     /* 40: IRQ24 TIM1_BRK/TIM9 */
    (uint32_t)TIM1_UP_TIM10_IRQHandler,     /* 41: IRQ25 TIM1_UP/TIM10 */
    (uint32_t)TIM1_TRG_COM_TIM11_IRQHandler,/* 42: IRQ26 TIM1_TRG/TIM11 */
    (uint32_t)TIM1_CC_IRQHandler,           /* 43: IRQ27 TIM1_CC */

    /* IRQ28–IRQ30: TIM2/3/4 */
    (uint32_t)TIM2_IRQHandler,              /* 44: IRQ28 TIM2 ← */
    (uint32_t)TIM3_IRQHandler,              /* 45: IRQ29 TIM3 ← */
    (uint32_t)TIM4_IRQHandler,              /* 46: IRQ30 TIM4 ← */

    /* IRQ31–IRQ36: I2C / SPI */
    (uint32_t)I2C1_EV_IRQHandler,           /* 47: IRQ31 I2C1_EV ← */
    (uint32_t)I2C1_ER_IRQHandler,           /* 48: IRQ32 I2C1_ER */
    (uint32_t)I2C2_EV_IRQHandler,           /* 49: IRQ33 I2C2_EV */
    (uint32_t)I2C2_ER_IRQHandler,           /* 50: IRQ34 I2C2_ER */
    (uint32_t)SPI1_IRQHandler,              /* 51: IRQ35 SPI1 ← */
    (uint32_t)SPI2_IRQHandler,              /* 52: IRQ36 SPI2 */

    /* IRQ37–IRQ38: USART */
    (uint32_t)USART1_IRQHandler,            /* 53: IRQ37 USART1 */
    (uint32_t)USART2_IRQHandler,            /* 54: IRQ38 USART2 */

    /* IRQ39: reserved */
    0,                                      /* 55: IRQ39 reserved */

    /* IRQ40–IRQ42 */
    (uint32_t)EXTI15_10_IRQHandler,         /* 56: IRQ40 EXTI15_10 */
    (uint32_t)RTC_Alarm_IRQHandler,         /* 57: IRQ41 RTC Alarm */
    (uint32_t)OTG_FS_WKUP_IRQHandler,       /* 58: IRQ42 OTG_FS wakeup */

    /* IRQ43–IRQ46: reserved */
    0, 0, 0,                                /* 59-61: IRQ43-45 reserved */

    /* IRQ46 */
    (uint32_t)DMA1_Stream7_IRQHandler,      /* 62: IRQ46 DMA1 S7 */

    /* IRQ47: reserved */
    0,                                      /* 63: IRQ47 reserved */

    /* IRQ48–IRQ50 */
    (uint32_t)SDIO_IRQHandler,              /* 64: IRQ48 SDIO */
    0,                                      /* 65: IRQ49 reserved */
    (uint32_t)TIM5_IRQHandler,              /* 66: IRQ50 TIM5 ← */

    /* IRQ51 */
    (uint32_t)SPI3_IRQHandler,              /* 67: IRQ51 SPI3 ← */

    /* IRQ52–IRQ55: reserved */
    0, 0, 0, 0,                             /* 68-71: IRQ52-55 reserved */

    /* IRQ56–IRQ60: DMA2 */
    (uint32_t)DMA2_Stream0_IRQHandler,      /* 72: IRQ56 DMA2 S0 */
    (uint32_t)DMA2_Stream1_IRQHandler,      /* 73: IRQ57 DMA2 S1 */
    (uint32_t)DMA2_Stream2_IRQHandler,      /* 74: IRQ58 DMA2 S2 */
    (uint32_t)DMA2_Stream3_IRQHandler,      /* 75: IRQ59 DMA2 S3 */
    (uint32_t)DMA2_Stream4_IRQHandler,      /* 76: IRQ60 DMA2 S4 */

    /* IRQ61–IRQ66 */
    0,                                      /* 77: IRQ61 reserved */
    0,                                      /* 78: IRQ62 reserved */
    (uint32_t)OTG_FS_IRQHandler,            /* 79: IRQ63 OTG_FS */
    (uint32_t)DMA2_Stream5_IRQHandler,      /* 80: IRQ64 DMA2 S5 */
    (uint32_t)DMA2_Stream6_IRQHandler,      /* 81: IRQ65 DMA2 S6 */
    (uint32_t)DMA2_Stream7_IRQHandler,      /* 82: IRQ66 DMA2 S7 */

    /* IRQ67–IRQ69: USART6, I2C3 */
    (uint32_t)USART6_IRQHandler,            /* 83: IRQ67 USART6 */
    (uint32_t)I2C3_EV_IRQHandler,           /* 84: IRQ68 I2C3_EV ← */
    (uint32_t)I2C3_ER_IRQHandler,           /* 85: IRQ69 I2C3_ER */

    /* IRQ70–IRQ80: reserved */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* 86-96: IRQ70-80 reserved */

    /* IRQ81: FPU */
    (uint32_t)FPU_IRQHandler,               /* 97: IRQ81 FPU */

    /* IRQ82–IRQ83: reserved */
    0, 0,                                   /* 98-99: reserved */

    /* IRQ84: SPI4 */
    (uint32_t)SPI4_IRQHandler,              /* 100: IRQ84 SPI4 */
};

/* ── Reset handler ──────────────────────────────────────────── */
void Reset_Handler(void)
{
    /* Copy .data from Flash to SRAM */
    volatile uint32_t *src = &_sidata;
    volatile uint32_t *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    /* Zero .bss */
    volatile uint32_t *bss = &_sbss;
    while (bss < &_ebss) *bss++ = 0;

    platform_init();
    main();
    while (1) {}
}

void Default_Handler(void)
{
    while (1) {}
}
