
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_csr.h"

#include <stdint.h>

struct rv_machine;

union rv_freg {
    float    f_32;
    double   f_64;
    uint32_t i_32;
    uint64_t i_64;
};

// One RISC-V CPU core's configuration and state.
struct rv_cpu {
    // Integer registers, the first is always zero and never written.
    uint64_t            xregs[32];
    // Floating-point registers.
    union rv_freg       fregs[32];
    // Control and status registers.
    struct rv_csr_state csr;
    // Program counter.
    uint64_t            pc;
};

// Execute one instruction word.
// Unlike `rv_step_insn`, this does not fetch on its own and only changes the PC for jumps and branches.
void rv_forcefeed_insn(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn);
// Fetch and execute one instruction.
void rv_step_insn(struct rv_machine *machine, struct rv_cpu *cpu);

// Read from an integer register.
[[gnu::always_inline]] static inline uint64_t rv_reg_read(struct rv_cpu *cpu, uint32_t index) {
    return cpu->xregs[index];
}

// Write to an integer register.
[[gnu::always_inline]] static inline void rv_reg_write(struct rv_cpu *cpu, uint32_t index, uint64_t value) {
    if (index != 0) {
        cpu->xregs[index] = value;
    }
}
