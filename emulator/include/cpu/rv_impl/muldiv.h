
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct emu_machine;
struct rv_cpu;

// Execute an M-extension instruction under the OP or OP-32 major opcodes
// (funct7 == 0x01).
void rv_muldiv_op(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
