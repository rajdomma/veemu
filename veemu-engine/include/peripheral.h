#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct veemu_board veemu_board_t;
typedef struct peripheral  peripheral_t;
typedef enum {
    PERIPH_TYPE_RCC=0, PERIPH_TYPE_GPIO=1, PERIPH_TYPE_UART=2,
    PERIPH_TYPE_SPI=3, PERIPH_TYPE_I2C=4, PERIPH_TYPE_TIMER=5,
    PERIPH_TYPE_NVIC=6, PERIPH_TYPE_SCB=7, PERIPH_TYPE_SYSTICK=8,
    PERIPH_TYPE_GENERIC=99,
} periph_type_t;
struct peripheral {
    char          name[32];
    periph_type_t type;
    uint8_t       instance;
    uint32_t      base_addr;
    uint32_t      size;
    void         *state;
    veemu_board_t *board;
    uint32_t (*read)   (peripheral_t *p, uint32_t offset);
    void     (*write)  (peripheral_t *p, uint32_t offset, uint32_t value);
    void     (*reset)  (peripheral_t *p);
    void     (*destroy)(peripheral_t *p);
};
#define VEEMU_FIFO_SIZE 4096
typedef struct {
    uint8_t  buf[VEEMU_FIFO_SIZE];
    uint32_t head, tail, count;
} veemu_fifo_t;
static inline void fifo_reset(veemu_fifo_t *f) { f->head=f->tail=f->count=0; }
static inline bool fifo_push(veemu_fifo_t *f, uint8_t b) {
    if (f->count>=VEEMU_FIFO_SIZE) return false;
    f->buf[f->tail]=b; f->tail=(f->tail+1)%VEEMU_FIFO_SIZE; f->count++; return true;
}
static inline bool fifo_pop(veemu_fifo_t *f, uint8_t *out) {
    if (!f->count) return false;
    *out=f->buf[f->head]; f->head=(f->head+1)%VEEMU_FIFO_SIZE; f->count--; return true;
}
static inline uint32_t fifo_count(const veemu_fifo_t *f) { return f->count; }
#ifdef __cplusplus
}
#endif
