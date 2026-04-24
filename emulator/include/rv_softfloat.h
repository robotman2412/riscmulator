
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_csr.h"
#include "rv_insn.h"

#include <stdint.h>

#include <softfloat.h>

// Map a RISC-V FRM value to the equivalent Berkeley SoftFloat rounding mode.
static inline uint_fast8_t rv_frm_to_softfloat(enum rv_frm frm) {
    switch (frm) {
        case RV_FRM_RNE: return softfloat_round_near_even;
        case RV_FRM_RTZ: return softfloat_round_minMag;
        case RV_FRM_RDN: return softfloat_round_min;
        case RV_FRM_RUP: return softfloat_round_max;
        case RV_FRM_RMM: return softfloat_round_near_maxMag;
        default: return softfloat_round_near_even;
    }
}

// Set the SoftFloat global rounding mode from a RISC-V FRM value.
static inline void rv_softfloat_setround(enum rv_frm frm) {
    softfloat_roundingMode = rv_frm_to_softfloat(frm);
}

// Clear the SoftFloat exception flags.
static inline void rv_softfloat_clearflags(void) {
    softfloat_exceptionFlags = 0;
}

// Return the current SoftFloat exception flags as RISC-V fflags bits.
// Berkeley SoftFloat's flag bit layout is identical to RISC-V fflags:
//   bit 0 = inexact (NX), bit 1 = underflow (UF), bit 2 = overflow (OF),
//   bit 3 = divide-by-zero (DZ), bit 4 = invalid (NV).
static inline uint8_t rv_softfloat_getflags(void) {
    return softfloat_exceptionFlags & RV_FFLAGS_MASK;
}
