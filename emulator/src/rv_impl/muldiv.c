
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/muldiv.h"

#include "rv_cpu.h"
#include "rv_insn.h"
#include "rv_machine.h"
#include "rv_privileged.h"

#include <stdint.h>

void rv_muldiv_op(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    bool    is_32  = RV_INSN_OP_MAJ(insn) & 0b00010;
    uint8_t funct3 = RV_INSN_FUNCT3(insn);

    // MULH/MULHSU/MULHU have no *W variants.
    if (is_32 && funct3 >= 1 && funct3 <= 3) {
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    uint64_t rs1 = rv_reg_read(cpu, RV_INSN_RS1(insn));
    uint64_t rs2 = rv_reg_read(cpu, RV_INSN_RS2(insn));

    int64_t rs1_s = (int64_t)rs1;
    int64_t rs2_s = (int64_t)rs2;

    if (is_32) {
        rs1_s = (int64_t)(int32_t)rs1;
        rs2_s = (int64_t)(int32_t)rs2;
        rs1   = (uint32_t)rs1;
        rs2   = (uint32_t)rs2;
    }

    uint64_t res;
    switch (funct3) {
        case 0: // MUL / MULW
            res = (uint64_t)(rs1_s * rs2_s);
            break;

        case 1: { // MULH: upper 64 bits of signed*signed
            __int128 prod = (__int128)rs1_s * (__int128)rs2_s;
            res           = (uint64_t)((unsigned __int128)prod >> 64);
            break;
        }

        case 2: { // MULHSU: upper 64 bits of signed*unsigned
            __int128 prod = (__int128)rs1_s * (unsigned __int128)rs2;
            res           = (uint64_t)((unsigned __int128)prod >> 64);
            break;
        }

        case 3: { // MULHU: upper 64 bits of unsigned*unsigned
            unsigned __int128 prod = (unsigned __int128)rs1 * (unsigned __int128)rs2;
            res                    = (uint64_t)(prod >> 64);
            break;
        }

        case 4: // DIV / DIVW
            if (rs2_s == 0) {
                res = (uint64_t)-1LL;
            } else if (!is_32 && rs1_s == INT64_MIN && rs2_s == -1) {
                res = (uint64_t)INT64_MIN;
            } else if (is_32 && rs1_s == (int64_t)INT32_MIN && rs2_s == -1) {
                res = (uint64_t)(int64_t)INT32_MIN;
            } else {
                res = (uint64_t)(rs1_s / rs2_s);
            }
            break;

        case 5: // DIVU / DIVUW
            if (rs2 == 0) {
                res = is_32 ? 0xFFFFFFFFULL : (uint64_t)-1ULL;
            } else {
                res = rs1 / rs2;
            }
            break;

        case 6: // REM / REMW
            if (rs2_s == 0) {
                res = (uint64_t)rs1_s;
            } else if (!is_32 && rs1_s == INT64_MIN && rs2_s == -1) {
                res = 0;
            } else if (is_32 && rs1_s == (int64_t)INT32_MIN && rs2_s == -1) {
                res = 0;
            } else {
                res = (uint64_t)(rs1_s % rs2_s);
            }
            break;

        case 7: // REMU / REMUW
            if (rs2 == 0) {
                res = rs1;
            } else {
                res = rs1 % rs2;
            }
            break;

        default:
            rv_do_iillegal(machine, cpu, insn);
            return;
    }

    if (is_32) {
        res = (uint64_t)(int64_t)(int32_t)res;
    }

    rv_reg_write(cpu, RV_INSN_RD(insn), res);
}
