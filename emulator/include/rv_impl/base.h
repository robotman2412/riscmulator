
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_machine;
struct rv_cpu;

// Execute an instruction under the OP, OP-IMM, OP-32 or OP-IMM-32 major opcodes.
void rv_base_op(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the LOAD or LOAD-FP major opcodes.
void rv_base_load(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the STORE or STORE-FP major opcodes.
void rv_base_store(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the JAL major opcode.
void rv_base_jal(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the JALR major opcode.
void rv_base_jalr(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the LUI or AUPIC major opcodes.
void rv_base_lui(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
