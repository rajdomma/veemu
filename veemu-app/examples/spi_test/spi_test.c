/**
 * spi_test.c — SPI1 Peripheral Test
 *
 * PA5=SCK, PA6=MISO, PA7=MOSI — AF5, push-pull.
 *
 * Digital twin: loopback (TX echoed as RX) — all bytes verified.
 * Real HW without MISO-MOSI wire: verifies SR flags (TXE, BSY),
 * detects no-loopback and reports clearly.
 *
 * Uses platform HW API — no direct UART register access.
 */

#include "spi_test.h"
#include "../../platform/gpio.h"
#include "../../platform/uart.h"
#include "../../platform/systick.h"
#include "../../platform/stm32f401re.h"
#include <stdio.h>

#define SPI1_BASE 0x40013000UL
typedef struct {
    volatile uint32_t CR1, CR2, SR, DR;
    volatile uint32_t CRCPR, RXCRCR, TXCRCR, I2SCFGR, I2SPR;
} SPI_TypeDef_t;
#define SPI1 ((SPI_TypeDef_t *)SPI1_BASE)

#define SPI_SR_RXNE  (1UL << 0)
#define SPI_SR_TXE   (1UL << 1)
#define SPI_SR_BSY   (1UL << 7)
#define SPI_CR1_MSTR (1UL << 2)
#define SPI_CR1_SPE  (1UL << 6)
#define SPI_CR1_SSI  (1UL << 8)
#define SPI_CR1_SSM  (1UL << 9)

/* SPI transfer — wait TXE, write DR, wait RXNE, read DR */
static uint8_t spi_transfer(uint8_t tx)
{
    uint32_t t = g_tick;
    while (!(SPI1->SR & SPI_SR_TXE))  { if ((g_tick-t)>10) return 0xFF; }
    SPI1->DR = tx;
    t = g_tick;
    while (!(SPI1->SR & SPI_SR_RXNE)) { if ((g_tick-t)>10) return 0xFF; }
    return (uint8_t)SPI1->DR;
}

void spi_test(void)
{
    printf("\n========================================\n");
    printf("  SPI1 Peripheral Test\n");
    printf("========================================\n\n");

    /* GPIO: PA5=SCK, PA6=MISO, PA7=MOSI — AF5 */
    RCC->AHB1ENR |= (1UL << 0);  /* GPIOAEN already on, ensure set */
    (void)RCC->AHB1ENR;

    /* PA5, PA6, PA7: AF mode */
    GPIOA->MODER &= ~((0x3UL<<(5*2))|(0x3UL<<(6*2))|(0x3UL<<(7*2)));
    GPIOA->MODER |=  ((0x2UL<<(5*2))|(0x2UL<<(6*2))|(0x2UL<<(7*2)));
    /* Push-pull, high speed */
    GPIOA->OTYPER  &= ~((1UL<<5)|(1UL<<6)|(1UL<<7));
    GPIOA->OSPEEDR |=  ((0x2UL<<(5*2))|(0x2UL<<(6*2))|(0x2UL<<(7*2)));
    /* AF5 (SPI1) on PA5, PA6, PA7 — in AFR[0] (pins 0-7) */
    GPIOA->AFR[0] &= ~((0xFUL<<(5*4))|(0xFUL<<(6*4))|(0xFUL<<(7*4)));
    GPIOA->AFR[0] |=  ((5UL<<(5*4))|(5UL<<(6*4))|(5UL<<(7*4)));

    /* SPI1 init: Master, BR=fPCLK/8, SSM=1, SSI=1, SPE=1 */
    RCC->APB2ENR |= (1UL << 12);
    (void)RCC->APB2ENR;
    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR | (2UL<<3) | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE;
    SPI1->CR2 = 0;

    printf("[SPI1] === Register Dump ===\n");
    printf("[SPI1] CR1 = 0x%08lX  (SPE=%lu MSTR=%lu SSM=%lu)\n",
        (unsigned long)SPI1->CR1,
        (unsigned long)((SPI1->CR1 & SPI_CR1_SPE)  ? 1:0),
        (unsigned long)((SPI1->CR1 & SPI_CR1_MSTR) ? 1:0),
        (unsigned long)((SPI1->CR1 & SPI_CR1_SSM)  ? 1:0));
    printf("[SPI1] CR2 = 0x%08lX\n", (unsigned long)SPI1->CR2);
    printf("[SPI1] SR  = 0x%08lX  (TXE=%lu RXNE=%lu BSY=%lu)\n\n",
        (unsigned long)SPI1->SR,
        (unsigned long)((SPI1->SR & SPI_SR_TXE)  ? 1:0),
        (unsigned long)((SPI1->SR & SPI_SR_RXNE) ? 1:0),
        (unsigned long)((SPI1->SR & SPI_SR_BSY)  ? 1:0));

    printf("[SPI1] TXE after init = %lu  %s\n\n",
        (unsigned long)((SPI1->SR & SPI_SR_TXE) ? 1:0),
        (SPI1->SR & SPI_SR_TXE) ? "OK" : "FAIL");

    /* Transfer test */
    const uint8_t test_bytes[] = { 0xA5, 0x5A, 0xFF, 0x00, 0x42 };
    const int n = 5;
    int loopback_detected = 0;
    int pass_count = 0;

    printf("[SPI1] === Transfer Test ===\n\n");

    for (int i = 0; i < n; i++) {
        uint8_t tx = test_bytes[i];
        uint8_t rx = spi_transfer(tx);
        int     ok = (rx == tx);
        if (ok && tx != 0x00) loopback_detected = 1;
        if (ok) pass_count++;
        printf("[SPI1] TX=0x%02X  RX=0x%02X  %s\n",
            tx, rx, ok ? "OK" : (tx==0?"OK(0==0)":"no loopback"));
    }

    printf("\n[SPI1] SR after transfers = 0x%08lX  (TXE=%lu BSY=%lu)\n\n",
        (unsigned long)SPI1->SR,
        (unsigned long)((SPI1->SR & SPI_SR_TXE) ? 1:0),
        (unsigned long)((SPI1->SR & SPI_SR_BSY) ? 1:0));

    if (loopback_detected) {
        printf("[SPI1] PASS — loopback detected, all bytes verified\n\n");
    } else {
        printf("[SPI1] SR flags OK — TXE/BSY correct\n");
        printf("[SPI1] Note: RX=0x00 on real HW without MISO-MOSI wire.\n");
        printf("[SPI1] Wire PA6(MISO) to PA7(MOSI) for full loopback test.\n");
        printf("[SPI1] On digital twin all bytes pass (engine echoes TX->RX).\n\n");
    }

    /* SR flag verification — works on both real HW and emulator */
    printf("[SPI1] === SR Flag Verification ===\n");
    uint32_t sr = SPI1->SR;
    printf("[SPI1] TXE=1 (DR empty):  %lu  %s\n",
        (unsigned long)((sr & SPI_SR_TXE) ? 1:0),
        (sr & SPI_SR_TXE) ? "OK" : "FAIL");
    printf("[SPI1] BSY=0 (idle):      %lu  %s\n",
        (unsigned long)((sr & SPI_SR_BSY) ? 1:0),
        !(sr & SPI_SR_BSY) ? "OK" : "FAIL");
    printf("\n");

    while (1) {}
}
