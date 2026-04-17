
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/base.h"

#include "rv_cpu.h"
#include "rv_insn.h"
#include "rv_machine.h"

// Execute an instruction under the OP, OP-IMM, OP-32 or OP-IMM-32 major opcodes.
void rv_base_op(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    (void)machine;

    uint64_t lhs = rv_reg_read(cpu, RV_INSN_RS1(insn));
    uint64_t rhs, rhs_uimm;
    if (RV_INSN_OP_MAJ(insn) & 0b01000) {
        // Is OP or OP-32.
        rhs = rhs_uimm = rv_reg_read(cpu, RV_INSN_RS2(insn));
    } else {
        // Is OP-IMM or OP-IMM-32.
        rhs      = RV_INSN_IMM12(insn);
        rhs_uimm = RV_INSN_UIMM12(insn);
    }

    // Used for signed operations.
    int64_t lhs_s = lhs;
    int64_t rhs_s = rhs;

    if (RV_INSN_OP_MAJ(insn) & 0b00010) {
        // Is OP-32 or OP-IMM-32; truncate inputs.
        lhs_s = ((int64_t)lhs << 32) >> 32;
        rhs_s = ((int64_t)lhs << 32) >> 32;
        lhs   = (uint32_t)lhs;
        rhs   = (uint32_t)rhs;
    }

    uint64_t res;
    switch (RV_INSN_FUNCT3(insn)) {
        case 0:
            if ((insn & (1 << 30)) && (RV_INSN_OP_MAJ(insn) & 0b01000)) {
                res = lhs - rhs; // sub
            } else {
                res = lhs + rhs; // add, addi
            }
            break;
        case 1: res = lhs << (rhs % 64); break; // sll, slli
        case 2: res = lhs_s < rhs_s; break;     // slt, slti
        case 3: res = lhs < rhs_uimm; break;    // sltu, sltiu
        case 4: res = lhs ^ rhs; break;         // xor, xori
        case 5:
            if (insn & (1 << 30)) {
                res = lhs_s >> (rhs % 64); // sra, srai
            } else {
                res = lhs >> (rhs % 64); // srl, srli
            }
            break;
        case 6: res = lhs | rhs; break; // or, ori
        case 7: res = lhs & rhs; break; // and, andi
        default: res = 0;               // Unreachable.
    }

    if (RV_INSN_OP_MAJ(insn) & 0b00010) {
        // Is OP-32 or OP-IMM-32; truncate result.
        res = ((int64_t)res << 32) >> 32;
    }

    rv_reg_write(cpu, RV_INSN_RD(insn), res);
}

// Execute an instruction under the LOAD or LOAD-FP major opcodes.
void rv_base_load(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    uint64_t addr = rv_reg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_IMM12(insn);

    uint64_t rdata;
    switch (RV_INSN_FUNCT3(insn)) {
        case 0: RV_READ_RAM(*machine, int8_t, addr, rdata, goto laccess;); break;
        case 1: RV_READ_RAM(*machine, int16_t, addr, rdata, goto laccess;); break;
        case 2: RV_READ_RAM(*machine, int32_t, addr, rdata, goto laccess;); break;
        case 4: RV_READ_RAM(*machine, uint8_t, addr, rdata, goto laccess;); break;
        case 5: RV_READ_RAM(*machine, uint16_t, addr, rdata, goto laccess;); break;
        case 6: RV_READ_RAM(*machine, uint32_t, addr, rdata, goto laccess;); break;
        case 3:
        case 7: RV_READ_RAM(*machine, uint64_t, addr, rdata, goto laccess;); break;
        default: rdata = 0; // Unreachable.
    }

    rv_reg_write(cpu, RV_INSN_RD(insn), rdata);

    return;
laccess:
    (void)0;
    // TODO: Set load access fault.
}

// Execute an instruction under the STORE or STORE-FP major opcodes.
void rv_base_store(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    uint64_t addr  = rv_reg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_S_IMM12(insn);
    uint64_t wdata = rv_reg_read(cpu, RV_INSN_RS2(insn));

    switch (RV_INSN_FUNCT3(insn)) {
        case 4:
        case 0: RV_WRITE_RAM(*machine, uint8_t, addr, wdata, goto saccess;); break;
        case 5:
        case 1: RV_WRITE_RAM(*machine, uint16_t, addr, wdata, goto saccess;); break;
        case 6:
        case 2: RV_WRITE_RAM(*machine, uint32_t, addr, wdata, goto saccess;); break;
        case 7:
        case 3: RV_WRITE_RAM(*machine, uint64_t, addr, wdata, goto saccess;); break;
        default: // Unreachable.
    }

    return;
saccess:
    (void)0;
    // TODO: Set store access fault.
}

// Execute an instruction under the JAL major opcode.
void rv_base_jal(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    (void)machine;

    // Unfortunately, this imm is harder to extract than usual.
    int32_t imm_19_12 = RV_INSN_BITFIELD(insn, 12, 0xff) << 12;
    int32_t imm_11    = RV_INSN_BITFIELD(insn, 20, 0x1) << 11;
    int32_t imm_10_1  = RV_INSN_BITFIELD(insn, 21, 0x3ff) << 1;
    int32_t imm_20    = (int32_t)insn >> 31 << 20;
    int32_t imm       = imm_19_12 | imm_11 | imm_10_1 | imm_20;

    // No need to check for IALIGN because this emulator has the C extension always enabled.
    rv_reg_write(cpu, RV_INSN_RD(insn), cpu->pc);
    cpu->pc = cpu->pc + imm;
}

// Execute an instruction under the JALR major opcode.
void rv_base_jalr(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    (void)machine;

    // No need to check for IALIGN because this emulator has the C extension always enabled.
    rv_reg_write(cpu, RV_INSN_RD(insn), cpu->pc);
    cpu->pc  = (int64_t)rv_reg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_IMM12(insn);
    cpu->pc &= ~1;
}

// Execute an instruction under the LUI or AUPIC major opcodes.
void rv_base_lui(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    (void)machine;

    uint64_t res = (int32_t)(insn & 0xfffff000);
    if (!(RV_INSN_OP_MAJ(insn) & 0b01000)) {
        res += cpu->pc; // auipc
    }

    rv_reg_write(cpu, RV_INSN_RD(insn), res);
}
