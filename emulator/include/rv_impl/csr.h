
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_cpu;

// Try to read a CSR; fails if no permission or does not exist.
bool rv_csr_read(struct rv_cpu *cpu, uint32_t index, uint64_t *rdata);
// Try to write a CSR; fails if no permission or does not exist.
// CSR writes do not fail for invalid values; instead, no action is taken.
bool rv_csr_write(struct rv_cpu *cpu, uint32_t index, uint64_t wdata);
