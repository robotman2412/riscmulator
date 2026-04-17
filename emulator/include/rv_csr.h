
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

// List of CSRs implemented by this emulator.
enum rv_csr {
#define RV_CSR_DEF(index, name) RV_CSR_##name = index,
#include "rv_defs/csr.h"
};

// Control and status register state.
struct rv_csr_state {
    // Note: `sstatus` uses a masked subset of this value.
    uint64_t mstatus;
    // Exception control: M-mode.
    uint64_t mie, mip, mideleg, medeleg, mtvec;
    // Exception status: M-mode.
    uint64_t mcause, mtval, mepc, mtinst;
    // Exception control: S-mode.
    uint64_t sie, sip, stvec;
    // Exception status: S-mode.
    uint64_t scause, stval, sepc;
    // Scratch registers.
    uint64_t sscratch, mscratch;
};

// Get the name of a CSR; returns `nullptr` if invalid.
[[gnu::const]] char const *rv_csr_to_name(enum rv_csr csr);
// Get a CSR by name; returns 0 if not found.
[[gnu::const]] enum rv_csr rv_csr_from_name(char const *name);
