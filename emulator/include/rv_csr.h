
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_cpu;

// PMP check granularity: Page sized.
#define RV_PMPGRAIN 12

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
    uint64_t mhartid, marchid, mimpid, mvendorid;
    // Note: `sstatus` uses a masked subset of this value.
    uint64_t mstatus;
    // Exception control: M-mode.
    uint64_t mie, mideleg, medeleg, mtvec;
    // Exception status: M-mode.
    uint64_t mcause, mtval, mepc, mtinst;

    // PMP configurations.
    union rv_pmpcfg pmpcfg;
    // PMP addresses.
    uint64_t        pmpaddr[64];

    // Exception control: S-mode.
    uint64_t sie, stvec;
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

#define RV_MISA_MASK(ext) ((uint64_t)1 << ((ext) - 'A'))
#define RV_MISA_VALUE                                                          \
    ((uint64_t)1 << 63 | RV_MISA_MASK('I') | RV_MISA_MASK('M') |               \
     RV_MISA_MASK('A') | RV_MISA_MASK('F') | RV_MISA_MASK('D') |               \
     RV_MISA_MASK('C'))

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
#define RV_STATUS_TVM_BIT      20
#define RV_STATUS_TW_BIT       21
#define RV_STATUS_TSR_BIT      22
#define RV_STATUS_UXL_BASE_BIT 32 // ,33
#define RV_STATUS_SXL_BASE_BIT 34 // ,35

#define RV_MSTATUS_HARDWIRED                                                   \
    ((uint64_t)2 << RV_STATUS_UXL_BASE_BIT | (uint64_t)2                       \
                                                 << RV_STATUS_SXL_BASE_BIT)

#define RV_MSTATUS_MASK                                                        \
    ((uint64_t)1 << RV_STATUS_SIE_BIT | (uint64_t)1 << RV_STATUS_MIE_BIT |     \
     (uint64_t)1 << RV_STATUS_SPIE_BIT | (uint64_t)1 << RV_STATUS_UBE_BIT |    \
     (uint64_t)1 << RV_STATUS_MPIE_BIT | (uint64_t)1 << RV_STATUS_SPP_BIT |    \
     (uint64_t)3 << RV_STATUS_VS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_MPP_BASE_BIT |                                   \
     (uint64_t)3 << RV_STATUS_FS_BASE_BIT |                                    \
     (uint64_t)3 << RV_STATUS_XS_BASE_BIT |                                    \
     (uint64_t)1 << RV_STATUS_MPRV_BIT | (uint64_t)1 << RV_STATUS_SUM_BIT |    \
     (uint64_t)1 << RV_STATUS_MXR_BIT | (uint64_t)1 << RV_STATUS_TVM_BIT |     \
     (uint64_t)1 << RV_STATUS_TW_BIT | (uint64_t)1 << RV_STATUS_TSR_BIT)

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

// Interrupt-pending bit positions (mip / sip / irq_pending).
#define RV_MIP_SSIP_BIT 1  // Supervisor software interrupt
#define RV_MIP_MSIP_BIT 3  // Machine software interrupt (CLINT-owned)
#define RV_MIP_STIP_BIT 5  // Supervisor timer interrupt
#define RV_MIP_MTIP_BIT 7  // Machine timer interrupt (CLINT-owned)
#define RV_MIP_SEIP_BIT 9  // Supervisor external interrupt
#define RV_MIP_MEIP_BIT 11 // Machine external interrupt (PLIC-owned)

// Bits writable via mip or sip CSR writes (hardware owns MSIP, MTIP, MEIP).
#define RV_SIP_WMASK                                                           \
    ((UINT64_C(1) << RV_MIP_SSIP_BIT) | (UINT64_C(1) << RV_MIP_STIP_BIT) |     \
     (UINT64_C(1) << RV_MIP_SEIP_BIT))

// PMP configuration byte bit fields.
#define RV_PMPCFG_R_BIT         0 // Read permission.
#define RV_PMPCFG_W_BIT         1 // Write permission.
#define RV_PMPCFG_X_BIT         2 // Execute permission.
#define RV_PMPCFG_A_BASE_BIT    3 // Address matching mode (2 bits).
#define RV_PMPCFG_L_BIT         7 // Locked.
// Address matching modes (stored in pmpcfg bits [4:3]).
#define RV_PMP_ADDR_MATCH_OFF   0
#define RV_PMP_ADDR_MATCH_TOR   1
#define RV_PMP_ADDR_MATCH_NA4   2
#define RV_PMP_ADDR_MATCH_NAPOT 3
// Valid bits in a pmpcfg byte; reserved bits [6:5] are hardwired to 0.
#define RV_PMPCFG_BYTE_MASK     0x9F
// In NAPOT mode, pmpaddr bits [G-2:0] read as all-ones.
#define RV_PMPGRAIN_NAPOT_MASK  ((1ULL << (RV_PMPGRAIN - 1)) - 1)
// In OFF/TOR mode, pmpaddr bits [G-1:0] read as all-zeros (one more bit than
// NAPOT).
#define RV_PMPGRAIN_OFF_MASK    ((1ULL << RV_PMPGRAIN) - 1)

#define RV_SATP_MODE_BASE_BIT 60
#define RV_SATP_ASID_BASE_BIT 44
#define RV_SATP_MODE_MASK     UINT64_C(0xf000000000000000)
#define RV_SATP_ASID_MASK     UINT64_C(0x0ffff00000000000)
#define RV_SATP_PPN_MASK      UINT64_C(0x00000fffffffffff)

// Get the name of a CSR; returns `nullptr` if invalid.
[[gnu::const]] char const *rv_csr_to_name(enum rv_csr csr);
// Get a CSR by name; returns 0 if not found.
[[gnu::const]] enum rv_csr rv_csr_from_name(char const *name);

// Try to read a CSR; fails if no permission or does not exist.
bool rv_csr_read(struct rv_cpu *cpu, uint32_t index, uint64_t *rdata);
// Try to write a CSR; fails if no permission or does not exist.
// CSR writes do not fail for invalid values; instead, no action is taken.
bool rv_csr_write(struct rv_cpu *cpu, uint32_t index, uint64_t wdata);
