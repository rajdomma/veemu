#pragma once
#include "peripheral.h"

peripheral_t *tim_create(uint8_t timer_id);
bool          tim_tick(peripheral_t *p, uint32_t insns, uint32_t cpu_hz);
bool          tim_is_pending(peripheral_t *p);
