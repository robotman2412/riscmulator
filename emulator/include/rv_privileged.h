
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_cpu;
struct rv_machine;

// RISC-V trap causes.
enum rv_cause {
    // Instruction address misaligned.
    RV_CAUSE_IALIGN   = 0,
    // Instruction access fault.
    RV_CAUSE_IACCESS  = 1,
    // Illegal instruction.
    RV_CAUSE_IILLEGAL = 2,
    // Breakpoint.
    RV_CAUSE_EBREAK   = 3,
    // Load address misaligned.
    RV_CAUSE_LALIGN   = 4,
    // Load access fault.
    RV_CAUSE_LACCESS  = 5,
    // Store address misaligned.
    RV_CAUSE_SALIGN   = 6,
    // Store access fault.
    RV_CAUSE_SACCESS  = 7,
    // E-call from U-mode.
    RV_CAUSE_ECALL_U  = 8,
    // E-call from S-mode.
    RV_CAUSE_ECALL_S  = 9,
    // E-call from M-mode.
    RV_CAUSE_ECALL_M  = 11,
    // Instruction page fault.
    RV_CAUSE_IPAGE    = 12,
    // Load page fault.
    RV_CAUSE_LPAGE    = 13,
    // Store page fault.
    RV_CAUSE_SPAGE    = 15,
    // Software check.
    RV_CAUSE_SWCHK    = 18,
    // Hardware error.
    RV_CAUSE_HWERR    = 19,
};

// RISC-V trap information.
struct rv_trap {
    // Value of `mepc` or `sepc`.
    uint64_t epc;
    // Value of `mtval` or `stval`; not used by interrupts.
    uint64_t tval;
    // Value of `mcause` or `scause`.
    uint64_t cause;
};

// Execute a certain trap handler.
void rv_do_trap(struct rv_machine *machine, struct rv_cpu *cpu, struct rv_trap trap);
// Execute the illegal instruction handler.
void rv_do_iillegal(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
