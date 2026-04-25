
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct emu_machine;
struct rv_cpu;

// Check PMP access permissions for a certain address and size.
// Returns the access permission bits seen in the same format as `pmpcfg`.
uint8_t rv_pmp_check(struct emu_machine *machine, struct rv_cpu *cpu, uint64_t paddr, uint64_t size, bool m_mode);
