
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct emu_machine;
struct rv_cpu;

// Execute an instruction under the OP, OP-IMM, OP-32 or OP-IMM-32 major opcodes.
void rv_base_op(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the LOAD or LOAD-FP major opcodes.
void rv_base_load(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the STORE or STORE-FP major opcodes.
void rv_base_store(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the JAL major opcode.
void rv_base_jal(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the JALR major opcode.
void rv_base_jalr(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the LUI or AUPIC major opcodes.
void rv_base_lui(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the SYSTEM major opcode.
void rv_base_system(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the BRANCH major opcode.
void rv_base_branch(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Execute an instruction under the MISC-MEM major opcode.
void rv_base_miscmem(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn);
