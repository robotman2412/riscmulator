// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "emu_machine.h"
#include "device/rv_plic.h"
#include "testcase.h"

#include <stdatomic.h>

// PLIC MMIO base address used across all tests (well above test RAM at 0x10000).
#define PLIC_BASE UINT64_C(0x0C000000)

// ---- Register-file tests (single CPU via TESTCASE) ----

// All registers read zero after initialisation.
TESTCASE(plic_init_zero, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    uint64_t val = 0xDEAD;

    // Priority for source 1 (offset 4).
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    // Pending word 0.
    val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x1000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    // Enable word 0 for context 0.
    val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    // Threshold for context 0.
    val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    rv_plic_destroy(&plic);
})

// Write and read back source priority registers.
TESTCASE(plic_source_priority_rw, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Source 1: offset 4.
    uint64_t val = 5;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 5);

    // Source 0 is reserved; writes are ignored and reads return 0.
    val = 7;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    rv_plic_destroy(&plic);
})

// rv_plic_set_pending sets the correct bit in the pending register.
TESTCASE(plic_pending_set_read, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    rv_plic_set_pending(&plic, 1);

    // Source 1 is bit 1 of pending word 0.
    uint64_t val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x1000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val & (1u << 1));

    // Source 0 is reserved; set_pending with 0 has no effect.
    rv_plic_set_pending(&plic, 0);
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x1000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(!(val & 1u));

    rv_plic_destroy(&plic);
})

// Write and read back per-context enable bits.
TESTCASE(plic_enable_rw, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Context 0 enable word 0 (offset 0x2000).
    uint64_t val = (1u << 1) | (1u << 3);
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == ((1u << 1) | (1u << 3)));

    rv_plic_destroy(&plic);
})

// Write and read back per-context threshold.
TESTCASE(plic_threshold_rw, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Context 0 threshold at offset 0x200000.
    uint64_t val = 4;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 4);

    rv_plic_destroy(&plic);
})

// Claim returns 0 when nothing is pending.
TESTCASE(plic_claim_empty, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    uint64_t val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    rv_plic_destroy(&plic);
})

// Claim returns the pending source when it is enabled and above threshold.
TESTCASE(plic_claim_basic, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Priority[1] = 1, enable source 1 for context 0, threshold = 0 (default).
    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    rv_plic_destroy(&plic);
})

// Claiming clears the pending bit.
TESTCASE(plic_claim_clears_pending, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x1000, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(!(val & (1u << 1)));

    rv_plic_destroy(&plic);
})

// Write to complete register; re-asserting the source allows a fresh claim.
TESTCASE(plic_complete, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    // Claim.
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    // Complete.
    val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);

    // Re-assert and claim again.
    rv_plic_set_pending(&plic, 1);
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    rv_plic_destroy(&plic);
})

// A source whose priority is <= threshold is not returned by claim.
TESTCASE(plic_priority_threshold, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Priority 2, threshold 3: 2 > 3 is false, so claim should return 0.
    uint64_t val = 2;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 3;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    val = 0xDEAD;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 0);

    rv_plic_destroy(&plic);
})

// With multiple pending sources, the highest-priority one is claimed first.
TESTCASE(plic_highest_priority_wins, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Source 1: priority 1.  Source 2: priority 5.
    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 5;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 8, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = (1u << 1) | (1u << 2);
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);
    rv_plic_set_pending(&plic, 2);

    // First claim: source 2 (priority 5).
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 2);

    // Second claim: source 1 (priority 1).
    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    rv_plic_destroy(&plic);
})

// PLIC sets MEIP in cpu->irq_pending for the M-mode context when an interrupt is pending.
TESTCASE(plic_mip_meip_set, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    TEST_ASSERT(atomic_load(&cpu->irq_pending) == 0);

    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    TEST_ASSERT(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT));

    rv_plic_destroy(&plic);
})

// PLIC sets SEIP in cpu->irq_pending for the S-mode context (context 1) when pending.
TESTCASE(plic_mip_seip_set, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    // Priority[1] = 1, enable source 1 only in context 1 (S-mode, hart 0).
    // Context 1 enable region: 0x2000 + 1*0x80 = 0x2080.
    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2080, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);

    TEST_ASSERT(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_PLIC_SEIP_BIT));
    TEST_ASSERT(!(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT)));

    rv_plic_destroy(&plic);
})

// MEIP is cleared after the pending source is claimed.
TESTCASE(plic_mip_cleared_on_claim, {
    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, machine));
    TEST_ASSERT(emu_machine_add_mmio(machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    rv_plic_set_pending(&plic, 1);
    TEST_ASSERT(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT));

    val = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, PLIC_BASE + 0x200004, &val, 2, RV_ACCESS_LOAD, false) == RV_MEM_OK);
    TEST_ASSERT(val == 1);

    TEST_ASSERT(!(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT)));

    rv_plic_destroy(&plic);
})

// ---- Multi-core test (manual machine init via TESTCASE_NOMACHINE) ----

// Two harts get independent interrupts through their respective M-mode contexts.
// Context 0 = hart 0 M, context 2 = hart 1 M.
TESTCASE_NOMACHINE(plic_multicore_contexts, {
    struct emu_machine machine = {0};
    TEST_ASSERT(emu_machine_init(&machine, 2, 0x10000, 0x1000));

    struct rv_plic plic = {0};
    TEST_ASSERT(rv_plic_init(&plic, &machine));
    TEST_ASSERT(emu_machine_add_mmio(&machine, rv_plic_mmio_region(&plic, PLIC_BASE)));

    struct rv_cpu *cpu0 = &machine.cpus[0];
    struct rv_cpu *cpu1 = &machine.cpus[1];

    // Source 1 priority 1, source 2 priority 1.
    uint64_t val = 1;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, PLIC_BASE + 4, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    val = 1;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, PLIC_BASE + 8, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);

    // Enable source 1 for context 0 (hart 0 M-mode: offset 0x2000).
    val = 1u << 1;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, PLIC_BASE + 0x2000, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);
    // Enable source 2 for context 2 (hart 1 M-mode: offset 0x2000 + 2*0x80 = 0x2100).
    val = 1u << 2;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, PLIC_BASE + 0x2100, &val, 2, RV_ACCESS_STORE, false) == RV_MEM_OK);

    // Trigger source 1: only hart 0 should be notified.
    rv_plic_set_pending(&plic, 1);
    TEST_ASSERT(atomic_load(&cpu0->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT));
    TEST_ASSERT(!(atomic_load(&cpu1->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT)));

    // Trigger source 2: hart 1 is now also notified; hart 0 still set.
    rv_plic_set_pending(&plic, 2);
    TEST_ASSERT(atomic_load(&cpu0->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT));
    TEST_ASSERT(atomic_load(&cpu1->irq_pending) & (UINT64_C(1) << RV_PLIC_MEIP_BIT));

    rv_plic_destroy(&plic);
    emu_machine_destroy(&machine);
})
