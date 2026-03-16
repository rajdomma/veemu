#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/peripheral.h"
#include "../include/veemu.h"

#define USART_SR   0x00
#define USART_DR   0x04
#define USART_BRR  0x08
#define USART_CR1  0x0C
#define USART_CR2  0x10
#define USART_CR3  0x14
#define USART_GTPR 0x18

#define SR_RXNE (1u<<5)
#define SR_TC   (1u<<6)
#define SR_TXE  (1u<<7)

typedef struct {
    uint32_t sr, dr, brr, cr1, cr2, cr3, gtpr;
    veemu_fifo_t tx_fifo, rx_fifo;
    veemu_uart_tx_cb_t tx_cb;
    void *tx_cb_ctx;
    uint8_t uart_id;
} uart_state_t;

static uint32_t uart_read(peripheral_t *p, uint32_t offset) {
    uart_state_t *s = p->state;
    if (offset == USART_SR) {
        if (fifo_count(&s->rx_fifo)>0) s->sr |= SR_RXNE;
        else s->sr &= ~SR_RXNE;
        return s->sr;
    }
    if (offset == USART_DR) {
        uint8_t b=0; fifo_pop(&s->rx_fifo,&b);
        if (!fifo_count(&s->rx_fifo)) s->sr &= ~SR_RXNE;
        return b;
    }
    switch(offset){
    case USART_BRR: return s->brr;
    case USART_CR1: return s->cr1;
    case USART_CR2: return s->cr2;
    case USART_CR3: return s->cr3;
    case USART_GTPR:return s->gtpr;
    default: return 0;
    }
}

static void uart_write(peripheral_t *p, uint32_t offset, uint32_t value) {
    uart_state_t *s = p->state;
    if (offset == USART_DR) {
        uint8_t byte = value & 0xFF;
        fifo_push(&s->tx_fifo, byte);
        if (s->tx_cb) s->tx_cb(s->uart_id, byte, s->tx_cb_ctx);
        s->sr |= SR_TXE | SR_TC;
        return;
    }
    switch(offset){
    case USART_SR:  s->sr=(s->sr&~SR_TC)|(value&SR_TC); break;
    case USART_BRR: s->brr=value;  break;
    case USART_CR1: s->cr1=value;  break;
    case USART_CR2: s->cr2=value;  break;
    case USART_CR3: s->cr3=value;  break;
    case USART_GTPR:s->gtpr=value; break;
    default: break;
    }
}

static void uart_reset(peripheral_t *p) {
    uart_state_t *s = p->state;
    s->sr=SR_TXE|SR_TC; s->dr=s->brr=s->cr1=s->cr2=s->cr3=s->gtpr=0;
    fifo_reset(&s->tx_fifo); fifo_reset(&s->rx_fifo);
}

static void uart_destroy(peripheral_t *p) { free(p->state); p->state=NULL; }

peripheral_t *uart_create(uint8_t uart_id) {
    peripheral_t *p = calloc(1,sizeof(peripheral_t));
    uart_state_t *s = calloc(1,sizeof(uart_state_t));
    if (!p||!s){free(p);free(s);return NULL;}
    s->uart_id=uart_id;
    fifo_reset(&s->tx_fifo); fifo_reset(&s->rx_fifo);
    snprintf(p->name,sizeof(p->name),"usart%d",uart_id);
    p->type=PERIPH_TYPE_UART; p->instance=uart_id;
    p->state=s; p->read=uart_read; p->write=uart_write;
    p->reset=uart_reset; p->destroy=uart_destroy;
    uart_reset(p); return p;
}

void uart_set_tx_cb(peripheral_t *p, veemu_uart_tx_cb_t cb, void *ctx) {
    uart_state_t *s=p->state; s->tx_cb=cb; s->tx_cb_ctx=ctx;
}
bool uart_rx_inject(peripheral_t *p, uint8_t byte) {
    uart_state_t *s=p->state;
    bool ok=fifo_push(&s->rx_fifo,byte);
    if(ok) s->sr|=SR_RXNE; return ok;
}
size_t uart_tx_drain(peripheral_t *p, uint8_t *buf, size_t len) {
    uart_state_t *s=p->state; size_t n=0; uint8_t b;
    while(n<len&&fifo_pop(&s->tx_fifo,&b)) buf[n++]=b; return n;
}
