/**
 * spi.c — SPI peripheral model (SPI1/SPI2/SPI3)
 *
 * Models:
 *   - CR1  SPE, MSTR, BR (baud rate), CPOL, CPHA, SSM, SSI, DFF, RXONLY
 *   - CR2  TXEIE, RXNEIE, ERRIE, SSOE
 *   - SR   TXE, RXNE, BSY
 *   - DR   data register — full-duplex loopback model
 *          write DR → fires tx_cb, echoes byte into RX FIFO (loopback)
 *          or device model can inject RX bytes via spi_rx_inject()
 *
 * Full-duplex: every write to DR simultaneously produces a read.
 * In loopback mode (no device attached) TX byte is echoed as RX.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../include/peripheral.h"

/* ── Register offsets (RM0368 Table 83) ──────────────────────── */
#define SPI_CR1    0x00
#define SPI_CR2    0x04
#define SPI_SR     0x08
#define SPI_DR     0x0C
#define SPI_CRCPR  0x10
#define SPI_RXCRCR 0x14
#define SPI_TXCRCR 0x18
#define SPI_I2SCFGR 0x1C
#define SPI_I2SPR  0x20

/* ── CR1 bits ─────────────────────────────────────────────────── */
#define CR1_CPHA   (1u << 0)
#define CR1_CPOL   (1u << 1)
#define CR1_MSTR   (1u << 2)
#define CR1_BR     (7u << 3)   /* baud rate [5:3] */
#define CR1_SPE    (1u << 6)
#define CR1_SSI    (1u << 8)
#define CR1_SSM    (1u << 9)
#define CR1_RXONLY (1u << 10)
#define CR1_DFF    (1u << 11)  /* 0=8bit 1=16bit */

/* ── SR bits ──────────────────────────────────────────────────── */
#define SR_RXNE  (1u << 0)
#define SR_TXE   (1u << 1)
#define SR_BSY   (1u << 7)

typedef struct {
    uint32_t cr1, cr2, sr, crcpr, rxcrcr, txcrcr, i2scfgr, i2spr;
    uint8_t  spi_id;
    bool     loopback;   /* echo TX→RX when no device attached */

    veemu_fifo_t rx_fifo;

    void (*tx_cb)(uint8_t spi_id, uint8_t byte, void *ctx);
    void *tx_cb_ctx;
} spi_state_t;

/* ── read ────────────────────────────────────────────────────── */
static uint32_t spi_read(peripheral_t *p, uint32_t offset)
{
    spi_state_t *s = p->state;
    switch (offset) {
    case SPI_CR1:  return s->cr1;
    case SPI_CR2:  return s->cr2;
    case SPI_SR:
        /* refresh RXNE from fifo */
        if (fifo_count(&s->rx_fifo)) s->sr |= SR_RXNE;
        else                          s->sr &= ~SR_RXNE;
        return s->sr;
    case SPI_DR: {
        uint8_t b = 0;
        if (fifo_pop(&s->rx_fifo, &b)) {
            if (!fifo_count(&s->rx_fifo)) s->sr &= ~SR_RXNE;
        }
        return b;
    }
    case SPI_CRCPR:   return s->crcpr;
    case SPI_RXCRCR:  return s->rxcrcr;
    case SPI_TXCRCR:  return s->txcrcr;
    case SPI_I2SCFGR: return s->i2scfgr;
    case SPI_I2SPR:   return s->i2spr;
    default: return 0;
    }
}

/* ── write ───────────────────────────────────────────────────── */
static void spi_write(peripheral_t *p, uint32_t offset, uint32_t value)
{
    spi_state_t *s = p->state;
    switch (offset) {
    case SPI_CR1:
        s->cr1 = value & 0xFFFF;
        break;
    case SPI_CR2:
        s->cr2 = value & 0xFF;
        break;
    case SPI_SR:
        /* only CRCERR is writable (RC_W0) */
        s->sr &= ~(1u << 4);
        s->sr |= value & (1u << 4);
        break;
    case SPI_DR: {
        if (!(s->cr1 & CR1_SPE)) break;  /* ignore if not enabled */
        uint8_t byte = value & 0xFF;

        /* fire TX callback */
        if (s->tx_cb) s->tx_cb(s->spi_id, byte, s->tx_cb_ctx);

        /* loopback: echo byte into RX if no device injected */
        if (s->loopback) {
            fifo_push(&s->rx_fifo, byte);
            s->sr |= SR_RXNE;   /* RX byte available immediately */
        }

        /* TX always ready immediately (no real clock) */
        s->sr |= SR_TXE;
        s->sr &= ~SR_BSY;
        break;
    }
    case SPI_CRCPR:   s->crcpr   = value; break;
    case SPI_I2SCFGR: s->i2scfgr = value; break;
    case SPI_I2SPR:   s->i2spr   = value; break;
    default: break;
    }
}

/* ── reset ───────────────────────────────────────────────────── */
static void spi_reset(peripheral_t *p)
{
    spi_state_t *s = p->state;
    s->cr1 = s->cr2 = 0;
    s->sr  = SR_TXE;    /* TXE=1, RXNE=0, BSY=0 on reset */
    s->crcpr = 0x0007;
    s->rxcrcr = s->txcrcr = 0;
    s->i2scfgr = 0; s->i2spr = 0x0002;
    fifo_reset(&s->rx_fifo);
}

static void spi_destroy(peripheral_t *p) { free(p->state); p->state = NULL; }

/* ── create ──────────────────────────────────────────────────── */
peripheral_t *spi_create(uint8_t spi_id)
{
    peripheral_t *p = calloc(1, sizeof(peripheral_t));
    spi_state_t  *s = calloc(1, sizeof(spi_state_t));
    if (!p || !s) { free(p); free(s); return NULL; }

    s->spi_id   = spi_id;
    s->loopback = true;   /* loopback by default until device attached */
    snprintf(p->name, sizeof(p->name), "spi%d", spi_id);
    p->type     = PERIPH_TYPE_SPI;
    p->instance = spi_id;
    p->state    = s;
    p->read     = spi_read;
    p->write    = spi_write;
    p->reset    = spi_reset;
    p->destroy  = spi_destroy;
    spi_reset(p);
    return p;
}

/* ── external API ────────────────────────────────────────────── */

/* Inject a byte into SPI RX FIFO (simulates slave MISO) */
bool spi_rx_inject(peripheral_t *p, uint8_t byte)
{
    spi_state_t *s = p->state;
    s->loopback = false;  /* device attached — disable loopback */
    bool ok = fifo_push(&s->rx_fifo, byte);
    if (ok) s->sr |= SR_RXNE;
    return ok;
}

/* Register TX callback (called when firmware writes DR) */
void spi_set_tx_cb(peripheral_t *p,
                   void (*cb)(uint8_t spi_id, uint8_t byte, void *ctx),
                   void *ctx)
{
    spi_state_t *s = p->state;
    s->tx_cb     = cb;
    s->tx_cb_ctx = ctx;
    s->loopback  = false;
}
