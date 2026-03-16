#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct veemu_ctx* veemu_handle_t;

veemu_handle_t  veemu_create        (const char* board_config_path);
int             veemu_load_elf      (veemu_handle_t h, const uint8_t* data, size_t size);
int             veemu_run           (veemu_handle_t h, uint64_t max_cycles);
void            veemu_stop          (veemu_handle_t h);
const uint8_t*  veemu_read_uart     (veemu_handle_t h, size_t* length);
void            veemu_gpio_snapshot (veemu_handle_t h, uint8_t* out);
void            veemu_destroy       (veemu_handle_t h);

#ifdef __cplusplus
}
#endif
