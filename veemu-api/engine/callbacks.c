#include "_cgo_export.h"
#include "veemu.h"
#include <stdint.h>
#include <stdbool.h>

static void c_uart_tx_cb(uint8_t uart_id, uint8_t b, void *ctx) {
    (void)uart_id;
    goUartByte(b, ctx);
}

static void c_gpio_cb(const char *port, uint8_t pin, bool state, void *ctx) {
    goGpioChange((char*)port, pin, (int)state, ctx);
}

void registerCallbacks(veemu_board_t *board) {
    veemu_set_uart_tx_cb(board, c_uart_tx_cb, (void*)board);
    veemu_set_gpio_cb(board,    c_gpio_cb,    (void*)board);
}
