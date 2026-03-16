/**
 * spi_test.c — SPI1 Peripheral Test
 *
 * Tests:
 *   1. Register readback after init (CR1, CR2, SR)
 *   2. TXE=1 after init (DR empty)
 *   3. Write byte to DR → loopback → RXNE=1
 *   4. Read back byte matches what was sent
 *   5. Multiple bytes: 0xA5, 0x5A, 0xFF, 0x00, 0x42
 *
 * SPI1 config:
 *   Master, BR=fPCLK/8, CPOL=0, CPHA=0, SSM=1, SSI=1, 8-bit
 *   Loopback mode active (no device attached — spi.c echoes TX→RX)
 */

#include "../../platform/stm32f401re.h"
#include "../../platform/systick.h"
#include "spi_test.h"
#include <stdint.h>

/* ── SPI1 registers ──────────────────────────────────────────── */
#define SPI1_BASE  0x40013000UL

typedef struct {
    volatile uint32_t CR1, CR2, SR, DR;
    volatile uint32_t CRCPR, RXCRCR, TXCRCR, I2SCFGR, I2SPR;
} SPI_TypeDef_t;

#define SPI1  ((SPI_TypeDef_t *) SPI1_BASE)

/* ── SR bits ─────────────────────────────────────────────────── */
#define SPI_SR_RXNE  (1UL << 0)
#define SPI_SR_TXE   (1UL << 1)
#define SPI_SR_BSY   (1UL << 7)

/* ── CR1 bits ────────────────────────────────────────────────── */
#define SPI_CR1_CPHA  (1UL << 0)
#define SPI_CR1_CPOL  (1UL << 1)
#define SPI_CR1_MSTR  (1UL << 2)
#define SPI_CR1_SPE   (1UL << 6)
#define SPI_CR1_SSI   (1UL << 8)
#define SPI_CR1_SSM   (1UL << 9)

/* ── UART helpers ────────────────────────────────────────────── */
static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER  &= ~(0x3UL << (2 * 2U));
    GPIOA->MODER  |=  (0x2UL << (2 * 2U));
    GPIOA->AFR[0] &= ~(0xFUL << (2 * 4U));
    GPIOA->AFR[0] |=  (7UL   << (2 * 4U));
    USART2->SR  = 0;
    USART2->CR1 = 0;
    USART2->BRR = 0x008BUL;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}
static void putc_(char c) {
    if (c == '\n') { while (!(USART2->SR & USART_SR_TXE)){} USART2->DR = '\r'; }
    while (!(USART2->SR & USART_SR_TXE)) {}
    USART2->DR = (uint8_t)c;
}
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void put_u(uint32_t v) {
    char buf[10]; int i = 0;
    if (!v) { putc_('0'); return; }
    while (v) { buf[i++] = '0' + v % 10; v /= 10; }
    while (i > 0) putc_(buf[--i]);
}
static void put_hex32(uint32_t v) {
    puts_("0x");
    for (int i = 7; i >= 0; i--)
        putc_("0123456789ABCDEF"[(v >> (i*4)) & 0xF]);
}
static void put_hex8(uint8_t v) {
    puts_("0x");
    putc_("0123456789ABCDEF"[(v >> 4) & 0xF]);
    putc_("0123456789ABCDEF"[v & 0xF]);
}

/* ── SPI transfer: send byte, return received byte ───────────── */
static uint8_t spi_transfer(uint8_t tx)
{
    /* wait TXE */
    uint32_t t = g_tick;
    while (!(SPI1->SR & SPI_SR_TXE)) {
        if ((g_tick - t) > 10) return 0xFF;
    }
    SPI1->DR = tx;

    /* wait RXNE */
    t = g_tick;
    while (!(SPI1->SR & SPI_SR_RXNE)) {
        if ((g_tick - t) > 10) return 0xFF;
    }
    return (uint8_t)SPI1->DR;
}

/* ── spi_test ────────────────────────────────────────────────── */
void spi_test(void)
{
    systick_init();
    uart_init();

    puts_("\n========================================\n");
    puts_("  SPI1 Peripheral Test\n");
    puts_("========================================\n\n");

    /* Enable SPI1 clock (APB2ENR bit 12) */
    RCC->APB2ENR |= (1UL << 12);
    (void)RCC->APB2ENR;

    /* Configure SPI1: Master, BR=fPCLK/8, SSM=1, SSI=1, SPE=1 */
    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR |   /* master */
                (2UL << 3)   |   /* BR: fPCLK/8 */
                SPI_CR1_SSM  |   /* software slave management */
                SPI_CR1_SSI  |   /* SSI=1 (NSS high) */
                SPI_CR1_SPE;     /* enable */
    SPI1->CR2 = 0;

    /* Register dump */
    puts_("[SPI1] === Register Dump ===\n");
    puts_("[SPI1] CR1 = "); put_hex32(SPI1->CR1);
    puts_("  (SPE="); putc_((SPI1->CR1 & SPI_CR1_SPE)  ? '1':'0');
    puts_(" MSTR=");   putc_((SPI1->CR1 & SPI_CR1_MSTR) ? '1':'0');
    puts_(" SSM=");    putc_((SPI1->CR1 & SPI_CR1_SSM)  ? '1':'0');
    puts_(")\n");
    puts_("[SPI1] CR2 = "); put_hex32(SPI1->CR2); puts_("\n");
    puts_("[SPI1] SR  = "); put_hex32(SPI1->SR);
    puts_("  (TXE="); putc_((SPI1->SR & SPI_SR_TXE)  ? '1':'0');
    puts_(" RXNE=");   putc_((SPI1->SR & SPI_SR_RXNE) ? '1':'0');
    puts_(" BSY=");    putc_((SPI1->SR & SPI_SR_BSY)  ? '1':'0');
    puts_(")\n\n");

    /* Verify TXE=1 after init */
    puts_("[SPI1] TXE after init = ");
    putc_((SPI1->SR & SPI_SR_TXE) ? '1' : '0');
    puts_((SPI1->SR & SPI_SR_TXE) ? "  OK\n" : "  FAIL\n");

    /* Transfer test */
    const uint8_t test_bytes[] = { 0xA5, 0x5A, 0xFF, 0x00, 0x42 };
    const int     n = 5;
    int           pass_count = 0;

    puts_("\n[SPI1] === Loopback Transfer Test ===\n");
    puts_("[SPI1] (TX byte echoed back as RX in loopback mode)\n\n");

    for (int i = 0; i < n; i++) {
        uint8_t tx  = test_bytes[i];
        uint8_t rx  = spi_transfer(tx);
        uint8_t ok  = (rx == tx);
        if (ok) pass_count++;

        puts_("[SPI1] TX="); put_hex8(tx);
        puts_("  RX=");     put_hex8(rx);
        puts_(ok ? "  OK\n" : "  FAIL (mismatch)\n");
    }

    puts_("\n[SPI1] SR after transfers = "); put_hex32(SPI1->SR);
    puts_("  (TXE="); putc_((SPI1->SR & SPI_SR_TXE) ? '1':'0');
    puts_(" RXNE=");  putc_((SPI1->SR & SPI_SR_RXNE) ? '1':'0');
    puts_(" BSY=");   putc_((SPI1->SR & SPI_SR_BSY)  ? '1':'0');
    puts_(")\n\n");

    if (pass_count == n)
        puts_("[SPI1] PASS -- all bytes looped back correctly\n\n");
    else {
        puts_("[SPI1] FAIL -- "); put_u((uint32_t)(n - pass_count));
        puts_(" bytes mismatched\n\n");
    }
}
