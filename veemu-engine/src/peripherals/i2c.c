/**
 * i2c.c — I2C peripheral model (I2C1/I2C2/I2C3)
 *
 * Models:
 *   - CR1  PE (peripheral enable), START, STOP, ACK
 *   - CR2  FREQ (APB clock in MHz)
 *   - OAR1 own address
 *   - SR1  SB (start bit), ADDR, TXE, RXNE, BTF, AF, BERR
 *   - SR2  MSL (master mode), BUSY, TRA
 *   - CCR  clock control
 *   - TRISE rise time
 *   - DR   data register — loopback model (write fires tx_cb, read returns rx)
 *
 * This is a functional register model — firmware can init I2C, write/read DR,
 * and poll SR1 flags. Actual bus simulation (ACK from slave device) requires
 * a device model to be attached via i2c_attach_device().
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../include/peripheral.h"

/* ── Register offsets (RM0368 Table 118) ─────────────────────── */
#define I2C_CR1   0x00
#define I2C_CR2   0x04
#define I2C_OAR1  0x08
#define I2C_OAR2  0x0C
#define I2C_DR    0x10
#define I2C_SR1   0x14
#define I2C_SR2   0x18
#define I2C_CCR   0x1C
#define I2C_TRISE 0x20
#define I2C_FLTR  0x24

/* ── CR1 bits ─────────────────────────────────────────────────── */
#define CR1_PE    (1u << 0)
#define CR1_START (1u << 8)
#define CR1_STOP  (1u << 9)
#define CR1_ACK   (1u << 10)
#define CR1_SWRST (1u << 15)

/* ── SR1 bits ─────────────────────────────────────────────────── */
#define SR1_SB    (1u << 0)   /* start bit generated */
#define SR1_ADDR  (1u << 1)   /* address sent/matched */
#define SR1_BTF   (1u << 2)   /* byte transfer finished */
#define SR1_TXE   (1u << 7)   /* DR empty (tx) */
#define SR1_RXNE  (1u << 6)   /* DR not empty (rx) */
#define SR1_AF    (1u << 10)  /* acknowledge failure */
#define SR1_BERR  (1u << 8)   /* bus error */

/* ── SR2 bits ─────────────────────────────────────────────────── */
#define SR2_MSL   (1u << 0)   /* master mode */
#define SR2_BUSY  (1u << 1)   /* bus busy */
#define SR2_TRA   (1u << 2)   /* transmitter */

typedef struct {
    uint32_t cr1, cr2, oar1, oar2, dr;
    uint32_t sr1, sr2, ccr, trise, fltr;
    uint8_t  i2c_id;

    /* simple rx FIFO — device pushes bytes here */
    veemu_fifo_t rx_fifo;

    /* tx callback — called when firmware writes DR */
    void (*tx_cb)(uint8_t i2c_id, uint8_t byte, void *ctx);
    void *tx_cb_ctx;
} i2c_state_t;

/* ── read ────────────────────────────────────────────────────── */
static uint32_t i2c_read(peripheral_t *p, uint32_t offset)
{
    i2c_state_t *s = p->state;
    switch (offset) {
    case I2C_CR1:  return s->cr1;
    case I2C_CR2:  return s->cr2;
    case I2C_OAR1: return s->oar1;
    case I2C_OAR2: return s->oar2;
    case I2C_DR: {
        uint8_t b = 0;
        fifo_pop(&s->rx_fifo, &b);
        if (!fifo_count(&s->rx_fifo)) s->sr1 &= ~SR1_RXNE;
        return b;
    }
    case I2C_SR1:
        /* reading SR1 does NOT clear flags — write 0 to clear */
        return s->sr1;
    case I2C_SR2:
        /* reading SR2 after SR1 clears ADDR (per STM32 spec) */
        if (s->sr1 & SR1_ADDR) s->sr1 &= ~SR1_ADDR;
        return s->sr2;
    case I2C_CCR:   return s->ccr;
    case I2C_TRISE: return s->trise;
    case I2C_FLTR:  return s->fltr;
    default: return 0;
    }
}

/* ── write ───────────────────────────────────────────────────── */
static void i2c_write(peripheral_t *p, uint32_t offset, uint32_t value)
{
    i2c_state_t *s = p->state;
    fprintf(stderr, "[i2c%d] write off=0x%02X val=0x%08X", s->i2c_id, offset, value);
    switch (offset) {
    case I2C_CR1:
        if (value & CR1_SWRST) {
            /* software reset — clear all state */
            s->cr1 = 0; s->sr1 = 0; s->sr2 = 0;
            break;
        }
        s->cr1 = value & 0xFFFF;

        if (value & CR1_START) {
            /* START condition generated */
            s->sr1 |= SR1_SB;
            s->sr2 |= SR2_MSL | SR2_BUSY;
            s->cr1 &= ~CR1_START;
            fprintf(stderr, "[i2c%d] START -> SR1=0x%02X SR2=0x%02X",
                    s->i2c_id, s->sr1, s->sr2);
        }
        if (value & CR1_STOP) {
            /* STOP condition — bus goes idle */
            s->sr2 &= ~(SR2_MSL | SR2_BUSY | SR2_TRA);
            s->sr1 &= ~(SR1_TXE | SR1_BTF);
            s->cr1 &= ~CR1_STOP;
            fprintf(stderr, "[i2c%d] STOP -> SR1=0x%02X SR2=0x%02X",
                    s->i2c_id, s->sr1, s->sr2);
        }
        break;

    case I2C_CR2:   s->cr2 = value & 0x7FF; break;
    case I2C_OAR1:  s->oar1 = value; break;
    case I2C_OAR2:  s->oar2 = value; break;

    case I2C_DR:
        if (s->sr1 & SR1_SB) {
            /* first write after START = address byte */
            s->sr1 &= ~SR1_SB;
            s->sr1 |= SR1_ADDR | SR1_TXE;
            if (value & 1) {
                /* read direction */
                s->sr2 &= ~SR2_TRA;
            } else {
                /* write direction */
                s->sr2 |= SR2_TRA;
                s->sr1 |= SR1_TXE;
            }
            /* simulate ACK from device — always ACK in model */
        } else {
            /* data byte TX */
            if (s->tx_cb) s->tx_cb(s->i2c_id, (uint8_t)(value & 0xFF), s->tx_cb_ctx);
            s->sr1 |= SR1_TXE | SR1_BTF;
        }
        break;

    case I2C_SR1:
        /* RC_W0: write 0 to clear error flags */
        s->sr1 &= (value | ~(SR1_AF | SR1_BERR));
        break;

    case I2C_CCR:   s->ccr   = value; break;
    case I2C_TRISE: s->trise = value; break;
    case I2C_FLTR:  s->fltr  = value; break;
    default: break;
    }
}

/* ── reset ───────────────────────────────────────────────────── */
static void i2c_reset(peripheral_t *p)
{
    i2c_state_t *s = p->state;
    memset(s, 0, sizeof(*s));
    s->sr1  = SR1_TXE;   /* DR empty on reset */
    s->trise = 0x02;
    fifo_reset(&s->rx_fifo);
}

static void i2c_destroy(peripheral_t *p) { free(p->state); p->state = NULL; }

/* ── create ──────────────────────────────────────────────────── */
peripheral_t *i2c_create(uint8_t i2c_id)
{
    peripheral_t *p = calloc(1, sizeof(peripheral_t));
    i2c_state_t  *s = calloc(1, sizeof(i2c_state_t));
    if (!p || !s) { free(p); free(s); return NULL; }

    s->i2c_id = i2c_id;
    snprintf(p->name, sizeof(p->name), "i2c%d", i2c_id);
    p->type     = PERIPH_TYPE_I2C;
    p->instance = i2c_id;
    p->state    = s;
    p->read     = i2c_read;
    p->write    = i2c_write;
    p->reset    = i2c_reset;
    p->destroy  = i2c_destroy;
    i2c_reset(p);
    return p;
}

/* ── external API ────────────────────────────────────────────── */

/* Push a byte into I2C RX FIFO (simulates slave sending data) */
bool i2c_rx_inject(peripheral_t *p, uint8_t byte)
{
    i2c_state_t *s = p->state;
    bool ok = fifo_push(&s->rx_fifo, byte);
    if (ok) s->sr1 |= SR1_RXNE;
    return ok;
}

/* Register TX callback (called when firmware writes DR) */
void i2c_set_tx_cb(peripheral_t *p,
                   void (*cb)(uint8_t i2c_id, uint8_t byte, void *ctx),
                   void *ctx)
{
    i2c_state_t *s = p->state;
    s->tx_cb     = cb;
    s->tx_cb_ctx = ctx;
}
