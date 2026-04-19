
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/atomics.h"

#include "rv_cpu.h"
#include "rv_insn.h"
#include "rv_machine.h"
#include "rv_privileged.h"

#include <stdint.h>

#include <pthread.h>

// AMO funct5 values per the RISC-V A extension spec.
#define AMO_FUNCT5(insn) (((insn) >> 27) & 0x1fu)

#define AMO_LR   0x02u
#define AMO_SC   0x03u
#define AMO_SWAP 0x01u
#define AMO_ADD  0x00u
#define AMO_XOR  0x04u
#define AMO_OR   0x08u
#define AMO_AND  0x0Cu
#define AMO_MIN  0x10u
#define AMO_MAX  0x14u
#define AMO_MINU 0x18u
#define AMO_MAXU 0x1Cu

void rv_atomic_op(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    uint8_t funct3 = RV_INSN_FUNCT3(insn);
    uint8_t funct5 = AMO_FUNCT5(insn);
    bool    is_32  = (funct3 == 2);

    // Only funct3 == 2 (.W) and funct3 == 3 (.D) are valid.
    if (funct3 != 2 && funct3 != 3) {
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    uint64_t addr       = rv_reg_read(cpu, RV_INSN_RS1(insn));
    uint64_t align_mask = is_32 ? 3u : 7u;

    if (addr & align_mask) {
        rv_do_trap(
            machine,
            cpu,
            (struct rv_trap){
                .epc   = cpu->epc,
                .cause = funct5 == AMO_LR ? RV_CAUSE_LALIGN : RV_CAUSE_SALIGN,
                .tval  = addr,
            }
        );
        return;
    }

    uint64_t mhartid = cpu->csr.mhartid;

    // LR.W / LR.D
    if (funct5 == AMO_LR) {
        if (RV_INSN_RS2(insn) != 0) {
            rv_do_iillegal(machine, cpu, insn);
            return;
        }

        pthread_mutex_lock(&machine->atomic_lock);

        uint64_t val;
        if (is_32) {
            int32_t v;
            RV_READ_RAM(*machine, int32_t, addr, v, {
                pthread_mutex_unlock(&machine->atomic_lock);
                rv_do_trap(
                    machine,
                    cpu,
                    (struct rv_trap){
                        .epc   = cpu->epc,
                        .cause = RV_CAUSE_LACCESS,
                        .tval  = addr,
                    }
                );
                return;
            });
            val = (int64_t)v;
        } else {
            RV_READ_RAM(*machine, uint64_t, addr, val, {
                pthread_mutex_unlock(&machine->atomic_lock);
                rv_do_trap(
                    machine,
                    cpu,
                    (struct rv_trap){
                        .epc   = cpu->epc,
                        .cause = RV_CAUSE_LACCESS,
                        .tval  = addr,
                    }
                );
                return;
            });
        }

        machine->reservations[mhartid] =
            (struct rv_reservation){.addr = addr, .valid = true};

        pthread_mutex_unlock(&machine->atomic_lock);

        rv_reg_write(cpu, RV_INSN_RD(insn), val);
        return;
    }

    // SC.W / SC.D
    if (funct5 == AMO_SC) {
        uint64_t rs2val = rv_reg_read(cpu, RV_INSN_RS2(insn));

        pthread_mutex_lock(&machine->atomic_lock);

        struct rv_reservation *res     = &machine->reservations[mhartid];
        bool                   success = res->valid && res->addr == addr;

        if (success) {
            if (is_32) {
                uint32_t v = (uint32_t)rs2val;
                RV_WRITE_RAM(*machine, uint32_t, addr, v, {
                    pthread_mutex_unlock(&machine->atomic_lock);
                    rv_do_trap(
                        machine,
                        cpu,
                        (struct rv_trap){
                            .epc   = cpu->epc,
                            .cause = RV_CAUSE_SACCESS,
                            .tval  = addr,
                        }
                    );
                    return;
                });
            } else {
                RV_WRITE_RAM(*machine, uint64_t, addr, rs2val, {
                    pthread_mutex_unlock(&machine->atomic_lock);
                    rv_do_trap(
                        machine,
                        cpu,
                        (struct rv_trap){
                            .epc   = cpu->epc,
                            .cause = RV_CAUSE_SACCESS,
                            .tval  = addr,
                        }
                    );
                    return;
                });
            }
            res->valid = false;
        }

        pthread_mutex_unlock(&machine->atomic_lock);

        rv_reg_write(cpu, RV_INSN_RD(insn), success ? 0u : 1u);
        return;
    }

    // AMO read-modify-write operations.
    uint64_t rs2val = rv_reg_read(cpu, RV_INSN_RS2(insn));

    pthread_mutex_lock(&machine->atomic_lock);

    uint64_t old;
    if (is_32) {
        int32_t v;
        RV_READ_RAM(*machine, int32_t, addr, v, {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .epc   = cpu->epc,
                    .cause = RV_CAUSE_SACCESS,
                    .tval  = addr,
                }
            );
            return;
        });
        old = (int64_t)v;
    } else {
        RV_READ_RAM(*machine, uint64_t, addr, old, {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .epc   = cpu->epc,
                    .cause = RV_CAUSE_SACCESS,
                    .tval  = addr,
                }
            );
            return;
        });
    }

    // Operands for comparison; .W ops use 32-bit sign/zero extension.
    uint64_t lhs   = is_32 ? (uint64_t)(uint32_t)old : old;
    uint64_t rhs   = is_32 ? (uint64_t)(uint32_t)rs2val : rs2val;
    int64_t  lhs_s = is_32 ? (int64_t)(int32_t)old : (int64_t)old;
    int64_t  rhs_s = is_32 ? (int64_t)(int32_t)rs2val : (int64_t)rs2val;

    uint64_t newval;
    switch (funct5) {
        case AMO_SWAP: newval = rhs; break;
        case AMO_ADD: newval = lhs + rhs; break;
        case AMO_XOR: newval = lhs ^ rhs; break;
        case AMO_OR: newval = lhs | rhs; break;
        case AMO_AND: newval = lhs & rhs; break;
        case AMO_MIN: newval = lhs_s < rhs_s ? lhs : rhs; break;
        case AMO_MAX: newval = lhs_s > rhs_s ? lhs : rhs; break;
        case AMO_MINU: newval = lhs < rhs ? lhs : rhs; break;
        case AMO_MAXU: newval = lhs > rhs ? lhs : rhs; break;
        default:
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_iillegal(machine, cpu, insn);
            return;
    }

    if (is_32) {
        uint32_t v = (uint32_t)newval;
        RV_WRITE_RAM(*machine, uint32_t, addr, v, {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .epc   = cpu->epc,
                    .cause = RV_CAUSE_SACCESS,
                    .tval  = addr,
                }
            );
            return;
        });
    } else {
        RV_WRITE_RAM(*machine, uint64_t, addr, newval, {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .epc   = cpu->epc,
                    .cause = RV_CAUSE_SACCESS,
                    .tval  = addr,
                }
            );
            return;
        });
    }

    // Invalidate any reservation that overlaps the written address.
    for (size_t i = 0; i < machine->cpu_count; i++) {
        if (machine->reservations[i].valid &&
            machine->reservations[i].addr == addr) {
            machine->reservations[i].valid = false;
        }
    }

    pthread_mutex_unlock(&machine->atomic_lock);

    rv_reg_write(cpu, RV_INSN_RD(insn), old);
}
