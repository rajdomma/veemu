#pragma once
#include "peripheral.h"

peripheral_t *i2c_create(uint8_t i2c_id);
bool          i2c_rx_inject(peripheral_t *p, uint8_t byte);
void          i2c_set_tx_cb(peripheral_t *p,
                  void (*cb)(uint8_t i2c_id, uint8_t byte, void *ctx),
                  void *ctx);
