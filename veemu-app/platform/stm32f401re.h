#pragma once
#include <stdint.h>

/* ── GPIO ─────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

/* ── USART ────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_TypeDef;

/* ── RCC ──────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR;
    volatile uint32_t AHB1RSTR, AHB2RSTR; uint32_t R0[2];
    volatile uint32_t APB1RSTR, APB2RSTR; uint32_t R1[2];
    volatile uint32_t AHB1ENR,  AHB2ENR;  uint32_t R2[2];
    volatile uint32_t APB1ENR,  APB2ENR;
} RCC_TypeDef;

/* ── Base addresses ───────────────────────────────────────── */
#define PERIPH_BASE   0x40000000UL
#define AHB1_BASE    (0x40020000UL)
#define APB1_BASE    (0x40000000UL)

#define GPIOA  ((GPIO_TypeDef  *)(AHB1_BASE + 0x0000UL))
#define GPIOB  ((GPIO_TypeDef  *)(AHB1_BASE + 0x0400UL))
#define GPIOC  ((GPIO_TypeDef  *)(AHB1_BASE + 0x0800UL))
#define USART2 ((USART_TypeDef *)(APB1_BASE + 0x4400UL))
#define RCC    ((RCC_TypeDef   *)(AHB1_BASE + 0x3800UL))

/* ── SysTick (ARMv7-M core peripheral) ───────────────────── */
#define SYST_CSR   (*((volatile uint32_t *)0xE000E010UL))
#define SYST_RVR   (*((volatile uint32_t *)0xE000E014UL))
#define SYST_CVR   (*((volatile uint32_t *)0xE000E018UL))
#define SYST_CALIB (*((volatile uint32_t *)0xE000E01CUL))

/* ── RCC bits ─────────────────────────────────────────────── */
#define RCC_AHB1ENR_GPIOAEN   (1UL << 0)
#define RCC_AHB1ENR_GPIOCEN   (1UL << 2)
#define RCC_APB1ENR_USART2EN  (1UL << 17)

/* ── USART bits ───────────────────────────────────────────── */
#define USART_SR_TXE   (1UL << 7)
#define USART_SR_RXNE  (1UL << 5)
#define USART_CR1_UE   (1UL << 13)
#define USART_CR1_TE   (1UL << 3)
#define USART_CR1_RE   (1UL << 2)
