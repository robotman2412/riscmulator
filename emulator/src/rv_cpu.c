
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"

#include "rv_impl/base.h"
#include "rv_insn.h"

#include <stdlib.h>

// Execute one instruction word.
// Unlike `rv_step_insn`, this does not fetch on its own and only changes the PC for jumps and branches.
void rv_forcefeed_insn(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    switch (RV_INSN_OP_MAJ(insn)) {
        case RV_OP_MAJ_OP:
        case RV_OP_MAJ_OP_IMM:
        case RV_OP_MAJ_OP_32:
        case RV_OP_MAJ_OP_IMM_32: rv_base_op(machine, cpu, insn); break;
        case RV_OP_MAJ_LOAD:
        case RV_OP_MAJ_LOAD_FP: rv_base_load(machine, cpu, insn); break;
        case RV_OP_MAJ_STORE:
        case RV_OP_MAJ_STORE_FP: rv_base_store(machine, cpu, insn); break;
        case RV_OP_MAJ_JAL: rv_base_jal(machine, cpu, insn); break;
        case RV_OP_MAJ_JALR: rv_base_jalr(machine, cpu, insn); break;
        case RV_OP_MAJ_LUI:
        case RV_OP_MAJ_AUIPC: rv_base_lui(machine, cpu, insn); break;
        default: abort(); // TODO: Do illegal instruction.
    }
}

// Fetch and execute one instruction.
void rv_step_insn(struct rv_machine *machine, struct rv_cpu *cpu) {
    (void)machine;
    (void)cpu;
}
