
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_machine;
struct rv_cpu;
struct rv_trap;

#define RV_TLB_WAYS 16
#define RV_TLB_ROWS 16

#define RV_PTE_V_BIT         0
#define RV_PTE_R_BIT         1
#define RV_PTE_W_BIT         2
#define RV_PTE_X_BIT         3
#define RV_PTE_U_BIT         4
#define RV_PTE_G_BIT         5
#define RV_PTE_A_BIT         6
#define RV_PTE_D_BIT         7
#define RV_PTE_PPN_BASE_BIT  10
#define RV_PTE_PPN_MASK      UINT64_C(0x003ffffffffffc00)
#define RV_PTE_RESERVED_MASK UINT64_C(0xffc0000000000000)

#define RV_TLB_PMP_MASK      0x7
#define RV_TLB_VPN_MASK      UINT64_C(0x0000003ffffff000)
#define RV_TLB_ASID_MASK     UINT64_C(0xffff000000000000)
#define RV_TLB_ASID_BASE_BIT 48

// One TLB entry.
struct rv_tlb_entry {
    // Virtual address, ASID and PMP permission bits.
    uint64_t vma;
    // Page Table Entry.
    uint64_t pte;
};

// Translation lookaside buffer.
struct rv_tlb {
    // TLB entries.
    struct rv_tlb_entry entries[RV_TLB_WAYS * RV_TLB_ROWS];
    // Per-row entry valid bits.
    uint16_t            valid[RV_TLB_ROWS];
};

// Look up a page-table entry without using the TLB.
// Writes to `pte_out`, or `trap_out` in the case of a fault while translating.
bool rv_paging_raw_lookup(
    struct rv_machine *machine,
    uint64_t           satp_value,
    uint64_t           vaddr,
    uint64_t          *pte_out,
    struct rv_trap    *trap_out
);
