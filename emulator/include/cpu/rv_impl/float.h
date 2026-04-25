
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct emu_machine;
struct rv_cpu;

// Do exact floating-point math instead of fast math.
extern bool rv_float_exact_math;


// Execute an instruction under the OP-FP major opcode.
void rv_float_op_fp(
    struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn
);
// Execute an instruction under the MADD, MSUB, NMADD or NMSUB major opcodes.
void rv_float_fmadd(
    struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn
);
