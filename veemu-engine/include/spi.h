#pragma once
#include "peripheral.h"

peripheral_t *spi_create(uint8_t spi_id);
bool          spi_rx_inject(peripheral_t *p, uint8_t byte);
void          spi_set_tx_cb(peripheral_t *p,
                  void (*cb)(uint8_t spi_id, uint8_t byte, void *ctx),
                  void *ctx);
