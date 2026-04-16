
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_csr.h"

#include <stdint.h>

union rv_freg {
    float    f_32;
    double   f_64;
    uint32_t i_32;
    uint64_t i_64;
};

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
