#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct veemu_board veemu_board_t;
typedef enum {
    VEEMU_OK=0, VEEMU_ERR_NOMEM=-1, VEEMU_ERR_ELF=-2,
    VEEMU_ERR_BOARD=-3, VEEMU_ERR_UC=-4, VEEMU_ERR_IO=-5, VEEMU_ERR_PARAM=-6,
} veemu_err_t;
typedef enum {
    VEEMU_RUN_OK=0, VEEMU_RUN_FAULT=1, VEEMU_RUN_HALTED=2,
} veemu_run_result_t;
typedef void (*veemu_uart_tx_cb_t)(uint8_t uart_id, uint8_t byte, void *ctx);
typedef void (*veemu_gpio_cb_t)(const char *port, uint8_t pin, bool state, void *ctx);
veemu_err_t        veemu_board_create  (const char *board_json, veemu_board_t **out);
veemu_err_t        veemu_board_load_elf(veemu_board_t *b, const char *elf_path);
veemu_err_t        veemu_board_reset   (veemu_board_t *b);
veemu_run_result_t veemu_board_run     (veemu_board_t *b, uint64_t max_insn);
void               veemu_board_free    (veemu_board_t *b);
void               veemu_set_uart_tx_cb(veemu_board_t *b, veemu_uart_tx_cb_t cb, void *ctx);
veemu_err_t        veemu_uart_rx_inject(veemu_board_t *b, uint8_t uart_id, uint8_t byte);
void               veemu_set_gpio_cb   (veemu_board_t *b, veemu_gpio_cb_t cb, void *ctx);
veemu_err_t        veemu_gpio_set_input(veemu_board_t *b, const char *port, uint8_t pin, bool state);
size_t             veemu_uart_drain    (veemu_board_t *b, uint8_t uart_id, uint8_t *buf, size_t len);
uint32_t           veemu_reg_read      (veemu_board_t *b, uint32_t addr);
const char        *veemu_strerror      (veemu_err_t err);
#ifdef __cplusplus
}
#endif
