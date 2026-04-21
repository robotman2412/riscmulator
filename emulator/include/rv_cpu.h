
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_csr.h"
#include "softfloat.h"

#include <inttypes.h>
#include <stdint.h>

struct rv_machine;

// Log-base 2 of page size.
#define RV_CPU_PAGE_SIZE_EXP 12
// Page size in bytes.
#define RV_CPU_PAGE_SIZE     (1 << RV_CPU_PAGE_SIZE_EXP)

// TODO: Assuming here the host is little-endian;
// alternative C23-compliant way to bit-cast for big-endian hosts?
union rv_freg {
    float     f_32;
    double    f_64;
    uint32_t  i_32;
    uint64_t  i_64;
    float32_t sf_32;
    float64_t sf_64;
};

// One RISC-V CPU core's configuration and state.
struct rv_cpu {
    // Integer registers, the first is always zero and never written.
    uint64_t            xregs[32];
    // Floating-point registers.
    union rv_freg       fregs[32];
    // Control and status registers.
    struct rv_csr_state csr;
    // Base address of the next instruction.
    uint64_t            pc;
    // Base address of the last loaded instruction.
    uint64_t            epc;
    // Current privilege level.
    uint8_t             privilege;
    // Set to true to stop this CPU's execution thread.
    bool                halted;
};

// Execute one instruction word.
// Unlike `rv_step_insn`, this does not fetch on its own and only changes the PC
// for jumps and branches.
void rv_forcefeed_insn(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
);
// Fetch and execute one instruction.
void rv_step_insn(struct rv_machine *machine, struct rv_cpu *cpu);

// Read from an integer register.
[[gnu::always_inline]] static inline uint64_t
    rv_xreg_read(struct rv_cpu *cpu, uint32_t index) {
    return cpu->xregs[index];
}

// Write to an integer register.
[[gnu::always_inline]] static inline void
    rv_xreg_write(struct rv_cpu *cpu, uint32_t index, uint64_t value) {
    if (index != 0) {
        cpu->xregs[index] = value;
    }
}

// Read from an integer register.
[[gnu::always_inline]] static inline union rv_freg
    rv_freg_read(struct rv_cpu *cpu, uint32_t index) {
    return cpu->fregs[index];
}

// Write to an integer register.
[[gnu::always_inline]] static inline void
    rv_freg_write(struct rv_cpu *cpu, uint32_t index, union rv_freg value) {
    cpu->fregs[index] = value;
}
