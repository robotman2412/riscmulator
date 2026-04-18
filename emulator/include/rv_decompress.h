
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

// Decompress a 16-bit instruction.
bool rv_decompress(uint16_t in, uint32_t *out);
