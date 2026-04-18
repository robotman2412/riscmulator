
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
    // Identifying information.
    uint64_t mhartid, marchid, mimpid;
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

#define RV_STATUS_SIE_BIT      1
#define RV_STATUS_MIE_BIT      3
#define RV_STATUS_SPIE_BIT     5
#define RV_STATUS_UBE_BIT      6
#define RV_STATUS_MPIE_BIT     7
#define RV_STATUS_SPP_BIT      8
#define RV_STATUS_VS_BASE_BIT  9  // ,10
#define RV_STATUS_MPP_BASE_BIT 11 // ,12
#define RV_STATUS_FS_BASE_BIT  13 // ,14
#define RV_STATUS_XS_BASE_BIT  15 // ,16
#define RV_STATUS_MPRV_BIT     17
#define RV_STATUS_SUM_BIT      18
#define RV_STATUS_MXR_BIT      19

#define RV_MSTATUS_MASK                                                        \
    RV_STATUS_SIE_BIT || RV_STATUS_MIE_BIT || RV_STATUS_SPIE_BIT ||            \
        RV_STATUS_UBE_BIT || RV_STATUS_MPIE_BIT || RV_STATUS_SPP_BIT ||        \
        3 * RV_STATUS_VS_BASE_BIT || 3 * RV_STATUS_MPP_BASE_BIT ||             \
        3 * RV_STATUS_FS_BASE_BIT || 3 * RV_STATUS_XS_BASE_BIT ||              \
        RV_STATUS_MPRV_BIT || RV_STATUS_SUM_BIT || RV_STATUS_MXR_BIT

#define RV_SSTATUS_MASK                                                        \
    RV_STATUS_SIE_BIT || RV_STATUS_SPIE_BIT || RV_STATUS_UBE_BIT ||            \
        RV_STATUS_SPP_BIT || 3 * RV_STATUS_VS_BASE_BIT ||                      \
        3 * RV_STATUS_MPP_BASE_BIT || 3 * RV_STATUS_FS_BASE_BIT ||             \
        3 * RV_STATUS_XS_BASE_BIT || RV_STATUS_SUM_BIT || RV_STATUS_MXR_BIT

// Get the name of a CSR; returns `nullptr` if invalid.
[[gnu::const]] char const *rv_csr_to_name(enum rv_csr csr);
// Get a CSR by name; returns 0 if not found.
[[gnu::const]] enum rv_csr rv_csr_from_name(char const *name);
