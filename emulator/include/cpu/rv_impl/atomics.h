
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_machine;
struct rv_cpu;

// Execute an instruction under the AMO major opcode.
void rv_atomic_op(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
