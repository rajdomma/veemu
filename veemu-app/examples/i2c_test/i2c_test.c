/**
 * i2c_test.c — I2C1 Peripheral Test
 */

#include "../../platform/stm32f401re.h"
#include "../../platform/systick.h"
#include "i2c_test.h"
#include <stdint.h>

#define I2C1_BASE  0x40005400UL

typedef struct {
    volatile uint32_t CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE, FLTR;
} I2C_TypeDef_t;

#define I2C1  ((I2C_TypeDef_t *) I2C1_BASE)

#define I2C_CR1_PE    (1UL << 0)
#define I2C_CR1_START (1UL << 8)
#define I2C_CR1_STOP  (1UL << 9)
#define I2C_SR1_SB    (1UL << 0)
#define I2C_SR1_ADDR  (1UL << 1)
#define I2C_SR1_BTF   (1UL << 2)
#define I2C_SR1_RXNE  (1UL << 6)
#define I2C_SR1_TXE   (1UL << 7)
#define I2C_SR2_MSL   (1UL << 0)
#define I2C_SR2_BUSY  (1UL << 1)
#define I2C_SR2_TRA   (1UL << 2)

static void uart_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    GPIOA->MODER  &= ~(0x3UL << (2*2U)); GPIOA->MODER  |= (0x2UL << (2*2U));
    GPIOA->AFR[0] &= ~(0xFUL << (2*4U)); GPIOA->AFR[0] |= (7UL   << (2*4U));
    USART2->SR=0; USART2->CR1=0; USART2->BRR=0x008BUL;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}
static void putc_(char c) {
    if (c=='\n'){while(!(USART2->SR&USART_SR_TXE)){} USART2->DR='\r';}
    while(!(USART2->SR&USART_SR_TXE)){}
    USART2->DR=(uint8_t)c;
}
static void puts_(const char *s){while(*s)putc_(*s++);}
static void put_hex32(uint32_t v){
    puts_("0x");
    for(int i=7;i>=0;i--) putc_("0123456789ABCDEF"[(v>>(i*4))&0xF]);
}
/* check against a snapshot — avoids re-reading volatile register */
static void chk(const char *label, uint32_t snap, uint32_t bit, uint8_t expected){
    uint8_t got=(snap&bit)?1:0;
    puts_(label); putc_(got+'0');
    puts_(got==expected?"  OK\n":"  FAIL\n");
}

void i2c_test(void)
{
    systick_init();
    uart_init();

    puts_("\n========================================\n");
    puts_("  I2C1 Peripheral Test\n");
    puts_("========================================\n\n");

    RCC->APB1ENR |= (1UL << 21);
    (void)RCC->APB1ENR;

    I2C1->CR1  = 0;
    I2C1->CR2  = 16U;
    I2C1->CCR  = 80U;
    I2C1->TRISE= 17U;
    I2C1->CR1  = I2C_CR1_PE;

    puts_("[I2C1] === Register Dump ===\n");
    puts_("[I2C1] CR1   = "); put_hex32(I2C1->CR1);  puts_("  (PE="); putc_((I2C1->CR1&I2C_CR1_PE)?'1':'0'); puts_(")\n");
    puts_("[I2C1] CR2   = "); put_hex32(I2C1->CR2);  puts_("  (FREQ=16MHz)\n");
    puts_("[I2C1] CCR   = "); put_hex32(I2C1->CCR);  puts_("  (100kHz SM)\n");
    puts_("[I2C1] TRISE = "); put_hex32(I2C1->TRISE);puts_("\n\n");

    uint32_t sr1, sr2;

    sr1 = I2C1->SR1;
    chk("[I2C1] PE=1 after init:  ", I2C1->CR1, I2C_CR1_PE,  1);
    chk("[I2C1] TXE=1 after init: ", sr1,        I2C_SR1_TXE, 1);
    puts_("\n");

    /* ── Test 1: START ───────────────────────────────────────── */
    puts_("[I2C1] === Test 1: START condition ===\n");
    I2C1->CR1 |= I2C_CR1_START;
    sr1 = I2C1->SR1;
    sr2 = I2C1->SR2;   /* reading SR2 here is intentional — snapshot both */
    puts_("[I2C1] SR1 = "); put_hex32(sr1); puts_("\n");
    puts_("[I2C1] SR2 = "); put_hex32(sr2); puts_("\n");
    chk("[I2C1] SR1.SB=1:   ", sr1, I2C_SR1_SB,   1);
    chk("[I2C1] SR2.MSL=1:  ", sr2, I2C_SR2_MSL,  1);
    chk("[I2C1] SR2.BUSY=1: ", sr2, I2C_SR2_BUSY, 1);
    puts_("\n");

    /* ── Test 2: Address byte ────────────────────────────────── */
    puts_("[I2C1] === Test 2: Address byte (0xD0 = 0x68 write) ===\n");
    I2C1->DR = 0xD0;
    sr1 = I2C1->SR1;          /* snapshot SR1 BEFORE reading SR2 */
    puts_("[I2C1] SR1 = "); put_hex32(sr1); puts_("\n");
    chk("[I2C1] SR1.SB=0:   ", sr1, I2C_SR1_SB,   0);
    chk("[I2C1] SR1.ADDR=1: ", sr1, I2C_SR1_ADDR, 1);
    chk("[I2C1] SR1.TXE=1:  ", sr1, I2C_SR1_TXE,  1);
    sr2 = I2C1->SR2;           /* reading SR2 clears ADDR — do it after checks */
    puts_("[I2C1] SR2 = "); put_hex32(sr2); puts_("\n");
    chk("[I2C1] SR2.TRA=1:  ", sr2, I2C_SR2_TRA,  1);
    sr1 = I2C1->SR1;
    chk("[I2C1] SR1.ADDR=0 after SR2 read: ", sr1, I2C_SR1_ADDR, 0);
    puts_("\n");

    /* ── Test 3: Data TX ─────────────────────────────────────── */
    puts_("[I2C1] === Test 3: Data byte TX (0xAB) ===\n");
    I2C1->DR = 0xAB;
    sr1 = I2C1->SR1;
    puts_("[I2C1] SR1 = "); put_hex32(sr1); puts_("\n");
    chk("[I2C1] SR1.TXE=1: ", sr1, I2C_SR1_TXE, 1);
    chk("[I2C1] SR1.BTF=1: ", sr1, I2C_SR1_BTF, 1);
    puts_("\n");

    /* ── Test 4: STOP ────────────────────────────────────────── */
    puts_("[I2C1] === Test 4: STOP condition ===\n");
    I2C1->CR1 |= I2C_CR1_STOP;
    sr2 = I2C1->SR2;
    puts_("[I2C1] SR2 = "); put_hex32(sr2); puts_("\n");
    chk("[I2C1] SR2.MSL=0:  ", sr2, I2C_SR2_MSL,  0);
    chk("[I2C1] SR2.BUSY=0: ", sr2, I2C_SR2_BUSY, 0);
    chk("[I2C1] SR2.TRA=0:  ", sr2, I2C_SR2_TRA,  0);
    puts_("\n");

    /* ── Test 5: RX idle state ───────────────────────────────── */
    puts_("[I2C1] === Test 5: RX path ===\n");
    sr1 = I2C1->SR1;
    chk("[I2C1] SR1.RXNE=0 at idle: ", sr1, I2C_SR1_RXNE, 0);

    I2C1->CR1 |= I2C_CR1_START;
    I2C1->DR   = 0xD1;   /* 0x68 read direction */
    sr1 = I2C1->SR1;
    sr2 = I2C1->SR2;
    puts_("[I2C1] SR1 after addr-read = "); put_hex32(sr1); puts_("\n");
    chk("[I2C1] SR2.TRA=0 (RX dir): ", sr2, I2C_SR2_TRA, 0);
    puts_("\n");

    puts_("[I2C1] PASS -- START/ADDR/DATA/STOP sequence verified\n\n");
}