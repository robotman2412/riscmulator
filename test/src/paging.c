
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"
#include "rv_paging.h"
#include "rv_pmp.h"
#include "testcase.h"

#include <string.h>

// Physical RAM for all VM tests: 64 KB at 0x80000000 (16 pages).
#define VM_RAM_BASE UINT64_C(0x80000000)
#define VM_RAM_SIZE 0x10000

// SATP values (mode=8 SV39).
//   A: ASID=0, root PPN=0x80000 (PA 0x80000000)
//   B: ASID=1, root PPN=0x80006 (PA 0x80006000)
#define SATP_A UINT64_C(0x8000000000080000)
#define SATP_B UINT64_C(0x8000100000080006)

// VM_TESTCASE: like TESTCASE but with a 64 KB machine at VM_RAM_BASE.
#define VM_TESTCASE(name, ...)                                                                                         \
    static bool _testbody_##name(struct rv_machine *machine, struct rv_cpu *cpu) {                                     \
        {                                                                                                              \
            __VA_ARGS__                                                                                                \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
    bool test_##name(void) {                                                                                           \
        struct rv_machine _machine = {0};                                                                              \
        if (!rv_machine_init(&_machine, 1, VM_RAM_BASE, VM_RAM_SIZE)) {                                                \
            testcase_error_message("rv_machine_init failed");                                                          \
            return false;                                                                                              \
        }                                                                                                              \
        bool _result = _testbody_##name(&_machine, &_machine.cpus[0]);                                                 \
        rv_machine_destroy(&_machine);                                                                                 \
        return _result;                                                                                                \
    }                                                                                                                  \
    [[gnu::constructor]] void _register_##name() {                                                                     \
        testcase_register(#name, test_##name);                                                                         \
    }

// Write a little-endian 64-bit value to physical RAM.
static void pte_write(struct rv_machine *machine, uint64_t paddr, uint64_t val) {
    uint8_t *p = machine->ram + (paddr - VM_RAM_BASE);
    p[0]       = (uint8_t)(val);
    p[1]       = (uint8_t)(val >> 8);
    p[2]       = (uint8_t)(val >> 16);
    p[3]       = (uint8_t)(val >> 24);
    p[4]       = (uint8_t)(val >> 32);
    p[5]       = (uint8_t)(val >> 40);
    p[6]       = (uint8_t)(val >> 48);
    p[7]       = (uint8_t)(val >> 56);
}

static uint64_t pte_read(struct rv_machine *machine, uint64_t paddr) {
    uint8_t *p = machine->ram + (paddr - VM_RAM_BASE);
    return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24 | (uint64_t)p[4] << 32 |
           (uint64_t)p[5] << 40 | (uint64_t)p[6] << 48 | (uint64_t)p[7] << 56;
}

// Build Layout A page tables and standard PMP in a freshly-inited machine.
// Sets cpu->privilege = 1 (S-mode) and satp = SATP_A.
//
// Layout A virtual mappings:
//   VA 0x1000 → PA 0x80003000  R|W          (data page 0)
//   VA 0x2000 → PA 0x80004000  R|W          (data page 1, cross-page boundary)
//   VA 0x3000 → PA 0x80005000  R|W          (data page 2, PMP leaf test)
//   VA 0x4000 → PA 0x80003000  R|W|G        (global, ASID test)
//
// PMP entry 0: locked NAPOT 64 KB at VM_RAM_BASE, RWX=7.
static void vm_setup(struct rv_machine *machine, struct rv_cpu *cpu) {
    // L2[0] → L1 at 0x80001000 (non-leaf, V only).
    pte_write(machine, 0x80000000, UINT64_C(0x0000000020000401));
    // L1[0] → L0 at 0x80002000 (non-leaf, V only).
    pte_write(machine, 0x80001000, UINT64_C(0x0000000020000801));
    // L0[1]: VA 0x1000 → data0 at 0x80003000, V|R|W (no A/D yet).
    pte_write(machine, 0x80002008, UINT64_C(0x0000000020000C07));
    // L0[2]: VA 0x2000 → data1 at 0x80004000, V|R|W.
    pte_write(machine, 0x80002010, UINT64_C(0x0000000020001007));
    // L0[3]: VA 0x3000 → data2 at 0x80005000, V|R|W.
    pte_write(machine, 0x80002018, UINT64_C(0x0000000020001407));
    // PMP entry 0: locked NAPOT 64 KB at VM_RAM_BASE, full permissions.
    cpu->csr.pmpcfg.unpacked[0] = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | 7;
    cpu->csr.pmpaddr[0]         = (VM_RAM_BASE >> 2) | 0x1FFF;

    cpu->csr.satp  = SATP_A;
    cpu->privilege = 1; // S-mode
}

// Build Layout B page tables into physical pages 6-8.
// VA 0x1000 is intentionally unmapped in Layout B.
static void vm_setup_b(struct rv_machine *machine) {
    // L2B[0] → L1B at 0x80007000.
    pte_write(machine, 0x80006000, UINT64_C(0x0000000020001C01));
    // L1B[0] → L0B at 0x80008000.
    pte_write(machine, 0x80007000, UINT64_C(0x0000000020002001));
    // L0B: entry 1 left as 0 — VA 0x1000 not mapped.
}

// ─── Simple load/store ────────────────────────────────────────────────────────

VM_TESTCASE(vm_load_simple, {
    vm_setup(machine, cpu);
    // Write known bytes to the target physical page.
    uint64_t expect = UINT64_C(0xDEADBEEFCAFEBABE);
    memcpy(machine->ram + 0x3000, &expect, 8);

    uint64_t got = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1000, &got, 3, RV_ACCESS_LOAD) == RV_MEM_OK)
    TEST_ASSERT(got == expect)
})

VM_TESTCASE(vm_store_simple, {
    vm_setup(machine, cpu);
    uint64_t val = UINT64_C(0x0102030405060708);
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1000, &val, 3, RV_ACCESS_STORE) == RV_MEM_OK)
    uint64_t got = 0;
    memcpy(&got, machine->ram + 0x3000, 8);
    TEST_ASSERT(got == val)
})

VM_TESTCASE(vm_no_mapping, {
    vm_setup(machine, cpu);
    // VA 0x0000 → L0[0] = 0 (not mapped).
    uint64_t dummy = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x0000, &dummy, 3, RV_ACCESS_LOAD) == RV_MEM_PAGE_FAULT)
})

// ─── A/D bits ─────────────────────────────────────────────────────────────────

VM_TESTCASE(vm_a_bit_load, {
    vm_setup(machine, cpu);
    // PTE initially has no A or D bits.
    uint64_t pte_before = pte_read(machine, 0x80002008);
    TEST_ASSERT((pte_before & (1 << RV_PTE_A_BIT)) == 0)
    TEST_ASSERT((pte_before & (1 << RV_PTE_D_BIT)) == 0)

    uint64_t dummy = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1000, &dummy, 3, RV_ACCESS_LOAD) == RV_MEM_OK)

    uint64_t pte_after = pte_read(machine, 0x80002008);
    TEST_ASSERT((pte_after & (1 << RV_PTE_A_BIT)) != 0) // A must be set
    TEST_ASSERT((pte_after & (1 << RV_PTE_D_BIT)) == 0) // D must stay clear
})

VM_TESTCASE(vm_a_d_bit_store, {
    vm_setup(machine, cpu);
    uint64_t pte_before = pte_read(machine, 0x80002008);
    TEST_ASSERT((pte_before & (1 << RV_PTE_A_BIT)) == 0)
    TEST_ASSERT((pte_before & (1 << RV_PTE_D_BIT)) == 0)

    uint64_t val = 0x42;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1000, &val, 3, RV_ACCESS_STORE) == RV_MEM_OK)

    uint64_t pte_after = pte_read(machine, 0x80002008);
    TEST_ASSERT((pte_after & (1 << RV_PTE_A_BIT)) != 0) // A must be set
    TEST_ASSERT((pte_after & (1 << RV_PTE_D_BIT)) != 0) // D must be set
})

// ─── Cross-page-boundary ──────────────────────────────────────────────────────

// 4-byte load at VA 0x1FFE crosses the boundary between the VA 0x1000 page
// (PA 0x80003000) and the VA 0x2000 page (PA 0x80004000).
// Bytes 0x80003FFE–0x80003FFF come from data0; 0x80004000–0x80004001 from data1.
VM_TESTCASE(vm_cross_page_load, {
    vm_setup(machine, cpu);
    machine->ram[0x3FFE] = 0xAA;
    machine->ram[0x3FFF] = 0xBB;
    machine->ram[0x4000] = 0xCC;
    machine->ram[0x4001] = 0xDD;

    uint32_t got = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1FFE, &got, 2, RV_ACCESS_LOAD) == RV_MEM_OK)
    TEST_ASSERT(got == 0xDDCCBBAA)
})

// 4-byte store at VA 0x1FFE: low bytes go to the VA 0x1000 page, high bytes to
// the VA 0x2000 page.
VM_TESTCASE(vm_cross_page_store, {
    vm_setup(machine, cpu);
    uint32_t val = 0x11223344;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1FFE, &val, 2, RV_ACCESS_STORE) == RV_MEM_OK)
    TEST_ASSERT(machine->ram[0x3FFE] == 0x44)
    TEST_ASSERT(machine->ram[0x3FFF] == 0x33)
    TEST_ASSERT(machine->ram[0x4000] == 0x22)
    TEST_ASSERT(machine->ram[0x4001] == 0x11)
})

// VA 0x3FFE: first two bytes land on the VA 0x3000 page (mapped), last two on
// VA 0x4000 (no L0 entry) → page fault.
VM_TESTCASE(vm_cross_page_fault, {
    vm_setup(machine, cpu);
    uint32_t dummy = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x3FFE, &dummy, 2, RV_ACCESS_LOAD) == RV_MEM_PAGE_FAULT)
})

// ─── PMP errors ───────────────────────────────────────────────────────────────

// Deny read on the L0 page table (PA 0x80002000).  The page walk must fail
// with ACCESS_FAULT before it can read any leaf PTE.
VM_TESTCASE(vm_pmp_page_table, {
    vm_setup(machine, cpu);
    // Entry 0: deny all on L0 table page (4 KB NAPOT, no perms).
    cpu->csr.pmpcfg.unpacked[0] = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | 0;
    cpu->csr.pmpaddr[0]         = (0x80002000 >> 2) | 0x1FF; // 4 KB
    // Entry 1: allow everything else in RAM.
    cpu->csr.pmpcfg.unpacked[1] = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | 7;
    cpu->csr.pmpaddr[1]         = (VM_RAM_BASE >> 2) | 0x1FFF; // 64 KB

    uint64_t dummy = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x1000, &dummy, 3, RV_ACCESS_LOAD) == RV_MEM_ACCESS_FAULT)
})

// Deny all access on the leaf data page (PA 0x80005000).  The walk completes
// but the cached PMP bits = 0, so check_access returns ACCESS_FAULT.
VM_TESTCASE(vm_pmp_leaf, {
    vm_setup(machine, cpu);
    // Entry 0: deny all on VA 0x3000 target page (4 KB NAPOT).
    cpu->csr.pmpcfg.unpacked[0] = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | 0;
    cpu->csr.pmpaddr[0]         = (0x80005000 >> 2) | 0x1FF; // 4 KB
    // Entry 1: allow everything else in RAM.
    cpu->csr.pmpcfg.unpacked[1] = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | 7;
    cpu->csr.pmpaddr[1]         = (VM_RAM_BASE >> 2) | 0x1FFF;

    uint64_t dummy = 0;
    TEST_ASSERT(rv_access_virt(machine, cpu, 0x3000, &dummy, 3, RV_ACCESS_LOAD) == RV_MEM_ACCESS_FAULT)
})

// ─── ASID isolation ───────────────────────────────────────────────────────────

// Access VA 0x1000 under Layout A (ASID=0), populating the TLB.  Switch to
// Layout B (ASID=1) where VA 0x1000 is not mapped.  The TLB must not serve the
// stale ASID=0 entry; rv_paging_lookup must either page-fault or return an
// entry with V=0.
VM_TESTCASE(vm_asid_no_stale, {
    vm_setup(machine, cpu);
    vm_setup_b(machine);

    // Warm up TLB with Layout A entry for VA 0x1000.
    struct rv_tlb_entry entry;
    TEST_ASSERT(rv_paging_lookup(machine, cpu, 0x1000, &entry, true, false) == RV_MEM_OK)
    TEST_ASSERT((entry.pte & (1 << RV_PTE_V_BIT)) != 0)

    // Switch to Layout B.
    cpu->csr.satp = SATP_B;

    struct rv_tlb_entry entry_b;
    enum rv_mem_result  res = rv_paging_lookup(machine, cpu, 0x1000, &entry_b, true, false);
    // Either a page fault, or an OK result with V=0 (no mapping).
    if (res == RV_MEM_OK) {
        TEST_ASSERT((entry_b.pte & (1 << RV_PTE_V_BIT)) == 0)
    } else {
        TEST_ASSERT(res == RV_MEM_PAGE_FAULT)
    }
})

// Map VA 0x4000 in Layout A with the G (global) bit set.  After populating the
// TLB under ASID=0, switch to ASID=1.  The global entry must still be served
// from the TLB regardless of ASID.
VM_TESTCASE(vm_asid_global_page, {
    vm_setup(machine, cpu);
    vm_setup_b(machine);

    // Add VA 0x4000 → data0 with G bit to Layout A (not part of base setup).
    // L0[4]: V|R|W|G = 0x27
    pte_write(machine, 0x80002020, UINT64_C(0x0000000020000C27));

    // Warm up TLB with the global entry for VA 0x4000 (ASID=0).
    struct rv_tlb_entry entry_a;
    TEST_ASSERT(rv_paging_lookup(machine, cpu, 0x4000, &entry_a, true, false) == RV_MEM_OK)
    TEST_ASSERT((entry_a.pte & (1 << RV_PTE_V_BIT)) != 0)
    TEST_ASSERT((entry_a.pte & (1 << RV_PTE_G_BIT)) != 0)

    // Switch to ASID=1 (Layout B — VA 0x4000 not in B's page table).
    cpu->csr.satp = SATP_B;

    // The global TLB entry must be served: valid, G bit set, same PPN.
    struct rv_tlb_entry entry_b;
    TEST_ASSERT(rv_paging_lookup(machine, cpu, 0x4000, &entry_b, true, false) == RV_MEM_OK)
    TEST_ASSERT((entry_b.pte & (1 << RV_PTE_V_BIT)) != 0)
    TEST_ASSERT((entry_b.pte & (1 << RV_PTE_G_BIT)) != 0)
    uint64_t ppn_a = (entry_a.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    uint64_t ppn_b = (entry_b.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    TEST_ASSERT(ppn_a == ppn_b)
})
