
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "emu_machine.h"

#include <stdint.h>

struct rv_cpu;
struct rv_trap;

#define RV_TLB_COLUMNS 16
#define RV_TLB_ROWS    16

#define RV_PTE_V_BIT         0
#define RV_PTE_R_BIT         1
#define RV_PTE_W_BIT         2
#define RV_PTE_X_BIT         3
#define RV_PTE_U_BIT         4
#define RV_PTE_G_BIT         5
#define RV_PTE_A_BIT         6
#define RV_PTE_D_BIT         7
#define RV_PTE_PPN_BASE_BIT  10
#define RV_PTE_RWX_MASK      UINT64_C(0x000000000000001E)
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
    struct rv_tlb_entry entries[RV_TLB_COLUMNS * RV_TLB_ROWS];
    // Per-row entry valid bits.
    uint16_t            valid[RV_TLB_ROWS];
    // Next column to replace in (global round-robin replacement policy).
    uint8_t             next_column;
};

// Look up a page-table entry without using the TLB.
// Sets A/D flags according to `mode` if the PTE and PMP grant access.
enum rv_mem_result rv_paging_raw_lookup(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, enum rv_access mode
);

// Do a cached lookup; try reading from the TLB first.
// Sets A/D flags according to `mode` if the PTE and PMP grant access.
enum rv_mem_result rv_paging_lookup(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, enum rv_access mode
);

// Access virtual memory.
enum rv_mem_result rv_access_virt(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, void *data, uint8_t size_exp, enum rv_access mode
);

// Clear the entire TLB.
void rv_flush_tlb(struct rv_cpu *cpu);

// Invalidate all entries with the ASID.
void rv_inval_tlb_asid(struct rv_cpu *cpu, uint16_t asid);

// Invalidate a specific virtual address.
void rv_inval_tlb_vaddr(struct rv_cpu *cpu, uint64_t vaddr, uint16_t asid, bool with_asid);

// Invalidate matching TLB entries.
void rv_inval_tlb(struct rv_cpu *cpu, uint64_t vaddr, uint16_t asid, bool with_vaddr, bool with_asid);

// Whether a virtual address is canonical.
bool rv_is_canon_vaddr(struct rv_cpu *cpu, uint64_t vaddr);
