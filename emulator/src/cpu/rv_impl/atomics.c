
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "cpu/rv_impl/atomics.h"

#include "cpu/rv_cpu.h"
#include "cpu/rv_insn.h"
#include "cpu/rv_privileged.h"
#include "emu_machine.h"

#include <pthread.h>
#include <stdint.h>

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

void rv_atomic_op(struct emu_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
    uint8_t funct3 = RV_INSN_FUNCT3(insn);
    uint8_t funct5 = AMO_FUNCT5(insn);
    bool    is_32  = (funct3 == 2);

    // Only funct3 == 2 (.W) and funct3 == 3 (.D) are valid.
    if (funct3 != 2 && funct3 != 3) {
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    uint64_t addr       = rv_xreg_read(cpu, RV_INSN_RS1(insn));
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

        uint64_t           val;
        enum rv_mem_result r;
        if (is_32) {
            uint32_t tmp = 0;
            r            = rv_access_virt(machine, cpu, addr, &tmp, 2, RV_ACCESS_LOAD);
            if (r != RV_MEM_OK) {
                pthread_mutex_unlock(&machine->atomic_lock);
                rv_do_trap(
                    machine,
                    cpu,
                    (struct rv_trap){
                        .cause = rv_mem_cause(r, RV_ACCESS_LOAD),
                        .epc   = cpu->epc,
                        .tval  = addr,
                    }
                );
                return;
            }
            val = (int64_t)(int32_t)tmp;
        } else {
            r = rv_access_virt(machine, cpu, addr, &val, 3, RV_ACCESS_LOAD);
            if (r != RV_MEM_OK) {
                pthread_mutex_unlock(&machine->atomic_lock);
                rv_do_trap(
                    machine,
                    cpu,
                    (struct rv_trap){
                        .cause = rv_mem_cause(r, RV_ACCESS_LOAD),
                        .epc   = cpu->epc,
                        .tval  = addr,
                    }
                );
                return;
            }
        }

        machine->reservations[mhartid] = (struct rv_reservation){.addr = addr, .valid = true};

        pthread_mutex_unlock(&machine->atomic_lock);

        rv_xreg_write(cpu, RV_INSN_RD(insn), val);
        return;
    }

    // SC.W / SC.D
    if (funct5 == AMO_SC) {
        uint64_t rs2val = rv_xreg_read(cpu, RV_INSN_RS2(insn));

        pthread_mutex_lock(&machine->atomic_lock);

        struct rv_reservation *res     = &machine->reservations[mhartid];
        bool                   success = res->valid && res->addr == addr;

        if (success) {
            enum rv_mem_result r;
            if (is_32) {
                uint32_t v = (uint32_t)rs2val;
                r          = rv_access_virt(machine, cpu, addr, &v, 2, RV_ACCESS_STORE);
                if (r != RV_MEM_OK) {
                    pthread_mutex_unlock(&machine->atomic_lock);
                    rv_do_trap(
                        machine,
                        cpu,
                        (struct rv_trap){
                            .cause = rv_mem_cause(r, RV_ACCESS_STORE),
                            .epc   = cpu->epc,
                            .tval  = addr,
                        }
                    );
                    return;
                }
            } else {
                r = rv_access_virt(machine, cpu, addr, &rs2val, 3, RV_ACCESS_STORE);
                if (r != RV_MEM_OK) {
                    pthread_mutex_unlock(&machine->atomic_lock);
                    rv_do_trap(
                        machine,
                        cpu,
                        (struct rv_trap){
                            .cause = rv_mem_cause(r, RV_ACCESS_STORE),
                            .epc   = cpu->epc,
                            .tval  = addr,
                        }
                    );
                    return;
                }
            }
            res->valid = false;
        }

        pthread_mutex_unlock(&machine->atomic_lock);

        rv_xreg_write(cpu, RV_INSN_RD(insn), success ? 0u : 1u);
        return;
    }

    // AMO read-modify-write operations.
    uint64_t rs2val = rv_xreg_read(cpu, RV_INSN_RS2(insn));

    pthread_mutex_lock(&machine->atomic_lock);

    uint64_t           old;
    enum rv_mem_result r;
    if (is_32) {
        uint32_t tmp = 0;
        r            = rv_access_virt(machine, cpu, addr, &tmp, 2, RV_ACCESS_AMO);
        if (r != RV_MEM_OK) {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .cause = rv_mem_cause(r, RV_ACCESS_AMO),
                    .epc   = cpu->epc,
                    .tval  = addr,
                }
            );
            return;
        }
        old = (int64_t)(int32_t)tmp;
    } else {
        r = rv_access_virt(machine, cpu, addr, &old, 3, RV_ACCESS_AMO);
        if (r != RV_MEM_OK) {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .cause = rv_mem_cause(r, RV_ACCESS_AMO),
                    .epc   = cpu->epc,
                    .tval  = addr,
                }
            );
            return;
        }
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
        r          = rv_access_virt(machine, cpu, addr, &v, 2, RV_ACCESS_STORE);
        if (r != RV_MEM_OK) {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .cause = rv_mem_cause(r, RV_ACCESS_STORE),
                    .epc   = cpu->epc,
                    .tval  = addr,
                }
            );
            return;
        }
    } else {
        r = rv_access_virt(machine, cpu, addr, &newval, 3, RV_ACCESS_STORE);
        if (r != RV_MEM_OK) {
            pthread_mutex_unlock(&machine->atomic_lock);
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .cause = rv_mem_cause(r, RV_ACCESS_STORE),
                    .epc   = cpu->epc,
                    .tval  = addr,
                }
            );
            return;
        }
    }

    // Invalidate any reservation that overlaps the written address.
    for (size_t i = 0; i < machine->cpu_count; i++) {
        if (machine->reservations[i].valid && machine->reservations[i].addr == addr) {
            machine->reservations[i].valid = false;
        }
    }

    pthread_mutex_unlock(&machine->atomic_lock);

    rv_xreg_write(cpu, RV_INSN_RD(insn), old);
}
