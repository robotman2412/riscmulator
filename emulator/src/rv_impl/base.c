
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/base.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_impl/csr.h"
#include "rv_impl/muldiv.h"
#include "rv_insn.h"
#include "rv_machine.h"
#include "rv_privileged.h"

#include <stdatomic.h>
#include <stdlib.h>

#include <sched.h>

// Execute an instruction under the OP, OP-IMM, OP-32 or OP-IMM-32 major
// opcodes.
void rv_base_op(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    if ((RV_INSN_OP_MAJ(insn) & 0b01000) && RV_INSN_FUNCT7(insn) == 0x01) {
        rv_muldiv_op(machine, cpu, insn);
        return;
    }

    uint64_t lhs = rv_xreg_read(cpu, RV_INSN_RS1(insn));
    uint64_t rhs, rhs_uimm;
    if (RV_INSN_OP_MAJ(insn) & 0b01000) {
        // Is OP or OP-32.
        rhs = rhs_uimm = rv_xreg_read(cpu, RV_INSN_RS2(insn));
    } else {
        // Is OP-IMM or OP-IMM-32.
        rhs      = RV_INSN_IMM12(insn);
        rhs_uimm = RV_INSN_UIMM12(insn);
    }

    // Used for signed operations.
    int64_t lhs_s = lhs;
    int64_t rhs_s = rhs;

    bool is_32 = RV_INSN_OP_MAJ(insn) & 0b00010;
    if (is_32) {
        // Is OP-32 or OP-IMM-32; truncate inputs.
        lhs_s    = ((int64_t)lhs << 32) >> 32;
        rhs_s    = ((int64_t)rhs << 32) >> 32;
        lhs      = (uint32_t)lhs;
        rhs      = (uint32_t)rhs;
        rhs_uimm = (uint32_t)rhs_uimm;
    }

    // W-type shifts use rs2[4:0]; 64-bit shifts use rs2[5:0].
    uint64_t shamt = rhs & (is_32 ? 0x1f : 0x3f);

    uint64_t res;
    switch (RV_INSN_FUNCT3(insn)) {
        case 0:
            if ((insn & (1 << 30)) && (RV_INSN_OP_MAJ(insn) & 0b01000)) {
                res = lhs - rhs; // sub
            } else {
                res = lhs + rhs; // add, addi
            }
            break;
        case 1: res = lhs << shamt; break;  // sll, slli, sllw, slliw
        case 2: res = lhs_s < rhs_s; break; // slt, slti
        case 3: res = lhs < rhs; break;     // sltu, sltiu
        case 4: res = lhs ^ rhs; break;     // xor, xori
        case 5:
            if (insn & (1 << 30)) {
                res = lhs_s >> shamt; // sra, srai, sraw, sraiw
            } else {
                res = lhs >> shamt; // srl, srli, srlw, srliw
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

    rv_xreg_write(cpu, RV_INSN_RD(insn), res);
}

// Execute an instruction under the LOAD or LOAD-FP major opcodes.
void rv_base_load(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    bool     is_load_fp = RV_INSN_OP_MAJ(insn) & 0b00001;
    uint64_t addr = rv_xreg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_IMM12(insn);

    if (is_load_fp && !RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
        // Float ops disabled.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }
    if (is_load_fp && (RV_INSN_FUNCT3(insn) & 0x6) != 0x2) {
        // Invalid size.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    uint64_t rdata;
    switch (RV_INSN_FUNCT3(insn)) {
        case 0:
            RV_READ_RAM(*machine, int8_t, addr, rdata, goto laccess;);
            break;
        case 1:
            RV_READ_RAM(*machine, int16_t, addr, rdata, goto laccess;);
            break;
        case 2:
            RV_READ_RAM(*machine, int32_t, addr, rdata, goto laccess;);
            break;
        case 3:
            RV_READ_RAM(*machine, int64_t, addr, rdata, goto laccess;);
            break;
        case 4:
            RV_READ_RAM(*machine, uint8_t, addr, rdata, goto laccess;);
            break;
        case 5:
            RV_READ_RAM(*machine, uint16_t, addr, rdata, goto laccess;);
            break;
        case 6:
            RV_READ_RAM(*machine, uint32_t, addr, rdata, goto laccess;);
            break;
        default: rv_do_iillegal(machine, cpu, insn); return;
    }

    if (is_load_fp) {
        if (RV_INSN_FUNCT3(insn) == 2) {
            // flw: NaN-box the 32-bit float in the upper 32 bits.
            rdata |= 0xffffffff00000000;
        }
        rv_freg_write(cpu, RV_INSN_RD(insn), (union rv_freg){.i_64 = rdata});
    } else {
        rv_xreg_write(cpu, RV_INSN_RD(insn), rdata);
    }

    return;
laccess:
    rv_do_trap(
        machine,
        cpu,
        (struct rv_trap){
            .cause = RV_CAUSE_LACCESS,
            .epc   = cpu->epc,
            .tval  = addr,
        }
    );
}

// Execute an instruction under the STORE or STORE-FP major opcodes.
void rv_base_store(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    bool     is_store_fp = RV_INSN_OP_MAJ(insn) & 0b00001;
    uint64_t addr =
        rv_xreg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_S_IMM12(insn);
    uint64_t wdata = is_store_fp ? rv_freg_read(cpu, RV_INSN_RS2(insn)).i_64
                                 : rv_xreg_read(cpu, RV_INSN_RS2(insn));

    if (is_store_fp && !RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
        // Float ops disabled.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }
    if (is_store_fp && (RV_INSN_FUNCT3(insn) & 0x6) != 0x2) {
        // Invalid size.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    switch (RV_INSN_FUNCT3(insn) & 3) {
        case 0:
            RV_WRITE_RAM(*machine, uint8_t, addr, wdata, goto saccess;);
            break;
        case 1:
            RV_WRITE_RAM(*machine, uint16_t, addr, wdata, goto saccess;);
            break;
        case 2:
            RV_WRITE_RAM(*machine, uint32_t, addr, wdata, goto saccess;);
            break;
        case 3:
            RV_WRITE_RAM(*machine, uint64_t, addr, wdata, goto saccess;);
            break;
        default: rv_do_iillegal(machine, cpu, insn); return;
    }

    return;
saccess:
    rv_do_trap(
        machine,
        cpu,
        (struct rv_trap){
            .cause = RV_CAUSE_SACCESS,
            .epc   = cpu->epc,
            .tval  = addr,
        }
    );
}

// Execute an instruction under the JAL major opcode.
void rv_base_jal(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    (void)machine;

    // Unfortunately, this imm is harder to extract than usual.
    int32_t imm_19_12 = RV_INSN_BITFIELD(insn, 12, 0xff) << 12;
    int32_t imm_11    = RV_INSN_BITFIELD(insn, 20, 0x1) << 11;
    int32_t imm_10_1  = RV_INSN_BITFIELD(insn, 21, 0x3ff) << 1;
    int32_t imm_20    = (int32_t)insn >> 31 << 20;
    int32_t imm       = imm_19_12 | imm_11 | imm_10_1 | imm_20;

    // No need to check for IALIGN because this emulator has the C extension
    // always enabled.
    rv_xreg_write(cpu, RV_INSN_RD(insn), cpu->pc);
    cpu->pc = cpu->epc + imm;
}

// Execute an instruction under the JALR major opcode.
void rv_base_jalr(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    (void)machine;

    // No need to check for IALIGN because this emulator has the C extension
    // always enabled.
    // Target must be read before writing rd, in case rs1 == rd.
    uint64_t target =
        ((int64_t)rv_xreg_read(cpu, RV_INSN_RS1(insn)) + RV_INSN_IMM12(insn)) &
        ~1ULL;
    rv_xreg_write(cpu, RV_INSN_RD(insn), cpu->pc);
    cpu->pc = target;
}

// Execute an instruction under the LUI or AUPIC major opcodes.
void rv_base_lui(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    (void)machine;

    uint64_t res = (int32_t)(insn & 0xfffff000);
    if (!(RV_INSN_OP_MAJ(insn) & 0b01000)) {
        res += cpu->epc; // auipc
    }

    rv_xreg_write(cpu, RV_INSN_RD(insn), res);
}

// Implementation of CSR operations.
static void
    do_csr(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    uint32_t csr = RV_INSN_UIMM12(insn);
    uint64_t wdata;
    bool     do_write;
    if (RV_INSN_FUNCT3(insn) & 4) {
        wdata    = RV_INSN_RS1(insn);
        do_write = true;
    } else {
        wdata = rv_xreg_read(cpu, RV_INSN_RS1(insn));
        // CSRRW always writes; CSRRS/CSRRC skip the write when rs1=x0.
        do_write = ((RV_INSN_FUNCT3(insn) & 3) == 1) || (RV_INSN_RS1(insn) != 0);
    }

    uint64_t rdata = 0;
    if (RV_INSN_RD(insn) != 0 || (RV_INSN_FUNCT3(insn) & 3) != 1) {
        if (!rv_csr_read(cpu, csr, &rdata)) {
            rv_do_iillegal(machine, cpu, insn);
            return;
        }
    }

    switch (RV_INSN_FUNCT3(insn) & 3) {
        case 0: rv_do_iillegal(machine, cpu, insn); return;
        case 1: /* Write verbatim. */ break;
        case 2: wdata = rdata | wdata; break;
        case 3: wdata = rdata & ~wdata; break;
    }

    if (do_write) {
        if (!rv_csr_write(cpu, csr, wdata)) {
            rv_do_iillegal(machine, cpu, insn);
            return;
        }
    }

    rv_xreg_write(cpu, RV_INSN_RD(insn), rdata);
}

// Execute an instruction under the SYSTEM major opcode.
void rv_base_system(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    if (RV_INSN_FUNCT3(insn) != 0) {
        do_csr(machine, cpu, insn);
    } else if (insn == 0x30200073 && cpu->privilege == 3) {
        // mret
        bool mpie         = (cpu->csr.mstatus >> RV_STATUS_MPIE_BIT) & 1;
        cpu->csr.mstatus &= ~(1llu << RV_STATUS_MIE_BIT);
        cpu->csr.mstatus |= mpie << RV_STATUS_MIE_BIT;
        cpu->privilege    = (cpu->csr.mstatus >> RV_STATUS_MPP_BASE_BIT) & 3;
        cpu->pc           = cpu->csr.mepc;
    } else if (insn == 0x00000073) {
        // ecall
        enum rv_cause cause;
        switch (cpu->privilege) {
            case 0: cause = RV_CAUSE_ECALL_U; break;
            case 1: cause = RV_CAUSE_ECALL_S; break;
            case 3: cause = RV_CAUSE_ECALL_M; break;
            default: abort(); // Unreachable.
        }
        rv_do_trap(
            machine,
            cpu,
            (struct rv_trap){
                .epc   = cpu->epc,
                .cause = cause,
                .tval  = 0,
            }
        );
    } else if (insn == 0x00100073) {
        // ebreak
        rv_do_trap(
            machine,
            cpu,
            (struct rv_trap){
                .epc   = cpu->epc,
                .cause = RV_CAUSE_EBREAK,
                .tval  = 0,
            }
        );
    } else {
        rv_do_trap(
            machine,
            cpu,
            (struct rv_trap){
                .epc   = cpu->epc,
                .cause = RV_CAUSE_IILLEGAL,
                .tval  = insn,
            }
        );
    }
}

// Execute an instruction under the BRANCH major opcode.
void rv_base_branch(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    (void)machine;

    // Like JAL, this IMM takes some more effort to extract.
    int32_t imm_4_1  = (int32_t)(insn & 0x00000f00) >> 8 << 1;
    int32_t imm_10_5 = (int32_t)(insn & 0x7e000000) >> 25 << 5;
    int32_t imm_11   = (int32_t)(insn & 0x00000080) >> 7 << 11;
    int32_t imm_12   = (int32_t)insn >> 31 << 12;
    int32_t imm      = imm_4_1 | imm_10_5 | imm_11 | imm_12;

    uint64_t lhs   = rv_xreg_read(cpu, RV_INSN_RS1(insn));
    uint64_t rhs   = rv_xreg_read(cpu, RV_INSN_RS2(insn));
    int64_t  lhs_s = lhs;
    int64_t  rhs_s = rhs;

    bool cond;
    switch (RV_INSN_FUNCT3(insn)) {
        case 0: cond = lhs == rhs; break;
        case 1: cond = lhs != rhs; break;
        case 4: cond = lhs_s < rhs_s; break;
        case 5: cond = lhs_s >= rhs_s; break;
        case 6: cond = lhs < rhs; break;
        case 7: cond = lhs >= rhs; break;
        default: rv_do_iillegal(machine, cpu, insn); return;
    }

    if (cond) {
        cpu->pc = imm + (int64_t)cpu->epc;
    }
}

// Execute an instruction under the MISC-MEM major opcode.
void rv_base_miscmem(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    (void)machine;
    (void)cpu;

    if (RV_INSN_FUNCT3(insn) == 1) {
        // Instruction-fetch fence.
        atomic_thread_fence(memory_order_acquire);
        return;
    }

    if (insn == 0x0100000f) {
        // Pause hint; yield execution to the system.
        sched_yield();
        return;
    }

    uint32_t const r  = 1;
    uint32_t const w  = 2;
    uint32_t const rw = 3;

    uint32_t pred = RV_INSN_BITFIELD(insn, 24, 0x3);
    uint32_t succ = RV_INSN_BITFIELD(insn, 20, 0x3);
    uint32_t fm   = RV_INSN_BITFIELD(insn, 28, 0xf);

    if (pred == r && succ == rw) {
        atomic_thread_fence(memory_order_acquire);
    } else if (pred == rw && succ == w) {
        atomic_thread_fence(memory_order_release);
    } else if (pred == rw && succ == rw && fm == 8) {
        // fence.tso
        atomic_thread_fence(memory_order_acq_rel);
    } else if (pred && succ) {
        // Note: Also the fallback for other fence orderings.
        atomic_thread_fence(memory_order_seq_cst);
    }
}
