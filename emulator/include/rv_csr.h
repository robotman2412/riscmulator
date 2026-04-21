
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_cpu;

// List of CSRs implemented by this emulator.
enum rv_csr {
#define RV_CSR_DEF(index, name) RV_CSR_##name = index,
#include "rv_defs/csr.h"
};

// PMP configurations.
union rv_pmpcfg {
    // PMP configurations (packed into 64-bit words).
    uint64_t packed[8];
    // PMP configurations (individual).
    uint8_t  unpacked[64];
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

    // PMP configurations.
    union rv_pmpcfg pmpcfg;
    // PMP addresses.
    uint64_t        pmpaddr[64];

    // Exception control: S-mode.
    uint64_t sie, sip, stvec;
    // Exception status: S-mode.
    uint64_t scause, stval, sepc;
    // Scratch registers.
    uint64_t sscratch, mscratch;
    // Virtual memory control.
    uint64_t satp;

    // Floating-point status and rounding mode.
    uint64_t fcsr;
};

// Extension state bits.
enum rv_xstate {
    // Disabled.
    RV_XSTATE_OFF     = 0b00,
    // Initial state (e.g. zeroed registers).
    RV_XSTATE_INITIAL = 0b01,
    // Clean state (no change since last set to clean).
    RV_XSTATE_CLEAN   = 0b10,
    // Dirty state (some changes).
    RV_XSTATE_DIRTY   = 0b11,
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
    ((uint64_t)1 << RV_STATUS_SIE_BIT | (uint64_t)1 << RV_STATUS_MIE_BIT |     \
     (uint64_t)1 << RV_STATUS_SPIE_BIT | (uint64_t)1 << RV_STATUS_UBE_BIT |    \
     (uint64_t)1 << RV_STATUS_MPIE_BIT | (uint64_t)1 << RV_STATUS_SPP_BIT |    \
     (uint64_t)3 << RV_STATUS_VS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_MPP_BASE_BIT |                                   \
     (uint64_t)3 << RV_STATUS_FS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_XS_BASE_BIT |                                    \
     (uint64_t)1 << RV_STATUS_MPRV_BIT | (uint64_t)1 << RV_STATUS_SUM_BIT |    \
     (uint64_t)1 << RV_STATUS_MXR_BIT)

#define RV_SSTATUS_MASK                                                        \
    ((uint64_t)1 << RV_STATUS_SIE_BIT | (uint64_t)1 << RV_STATUS_SPIE_BIT |    \
     (uint64_t)1 << RV_STATUS_UBE_BIT | (uint64_t)1 << RV_STATUS_SPP_BIT |     \
     (uint64_t)3 << RV_STATUS_VS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_FS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_XS_BASE_BIT | (uint64_t)1 << RV_STATUS_SUM_BIT | \
     (uint64_t)1 << RV_STATUS_MXR_BIT)

// Check extension enable bits.
#define RV_CHECK_XS(bits, xs_base_bit)                                         \
    ((((bits) >> (xs_base_bit)) & 3) != RV_XSTATE_OFF)

// Invalid operation.
#define RV_FFLAGS_NV_BIT 4
// Division by zero.
#define RV_FFLAGS_DZ_BIT 3
// Overflow.
#define RV_FFLAGS_OF_BIT 2
// Underflow.
#define RV_FFLAGS_UF_BIT 1
// Inexact operation.
#define RV_FFLAGS_NX_BIT 0

// Position of rounding mode in `fcsr`.
#define RV_FCSR_FRM_BASE_BIT 5

// Implemented bits in `fflags`.
#define RV_FFLAGS_MASK 0x1f
// Implemented bits in `frm`.
#define RV_FRM_MASK    0x7
// Implemented bits in `fcsr`.
#define RV_FCSR_MASK   (RV_FRM_MASK << RV_FCSR_FRM_BASE_BIT | RV_FFLAGS_MASK)

// Get the name of a CSR; returns `nullptr` if invalid.
[[gnu::const]] char const *rv_csr_to_name(enum rv_csr csr);
// Get a CSR by name; returns 0 if not found.
[[gnu::const]] enum rv_csr rv_csr_from_name(char const *name);

// Try to read a CSR; fails if no permission or does not exist.
bool rv_csr_read(struct rv_cpu *cpu, uint32_t index, uint64_t *rdata);
// Try to write a CSR; fails if no permission or does not exist.
// CSR writes do not fail for invalid values; instead, no action is taken.
bool rv_csr_write(struct rv_cpu *cpu, uint32_t index, uint64_t wdata);
