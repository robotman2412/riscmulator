
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"

#include "rv_decompress.h"
#include "rv_impl/atomics.h"
#include "rv_impl/base.h"
#include "rv_impl/float.h"
#include "rv_insn.h"
#include "rv_machine.h"
#include "rv_privileged.h"

// Execute one instruction word.
// Unlike `rv_step_insn`, this does not fetch on its own and only changes the PC
// for jumps and branches.
void rv_forcefeed_insn(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
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
        case RV_OP_MAJ_SYSTEM: rv_base_system(machine, cpu, insn); break;
        case RV_OP_MAJ_BRANCH: rv_base_branch(machine, cpu, insn); break;
        case RV_OP_MAJ_MISC_MEM: rv_base_miscmem(machine, cpu, insn); break;
        case RV_OP_MAJ_AMO: rv_atomic_op(machine, cpu, insn); break;
        case RV_OP_MAJ_OP_FP: rv_float_op_fp(machine, cpu, insn); break;
        case RV_OP_MAJ_MADD:
        case RV_OP_MAJ_NMADD:
        case RV_OP_MAJ_MSUB:
        case RV_OP_MAJ_NMSUB: rv_float_fmadd(machine, cpu, insn); break;
        default: rv_do_iillegal(machine, cpu, insn); break;
    }
}

// Fetch and execute one instruction.
void rv_step_insn(struct rv_machine *machine, struct rv_cpu *cpu) {
    uint16_t hw;
    if (!rv_access_phys(machine, cpu, cpu->pc, &hw, 1, RV_ACCESS_INSN)) return;

    uint32_t insn;
    if ((hw & 0x3u) != 0x3u) {
        // Compressed 16-bit instruction.
        if (!rv_decompress(hw, &insn)) {
            cpu->epc = cpu->pc;
            rv_do_iillegal(machine, cpu, hw);
            return;
        }
        cpu->epc  = cpu->pc;
        cpu->pc  += 2;
    } else {
        // Full 32-bit instruction: fetch the upper halfword.
        uint16_t hw2;
        uint64_t pc2 = cpu->pc + 2;
        if (!rv_access_phys(machine, cpu, pc2, &hw2, 1, RV_ACCESS_INSN)) return;
        insn      = (uint32_t)hw | ((uint32_t)hw2 << 16);
        cpu->epc  = cpu->pc;
        cpu->pc  += 4;
    }

    rv_forcefeed_insn(machine, cpu, insn);
}
