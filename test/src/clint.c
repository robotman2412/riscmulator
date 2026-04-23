// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_clint.h"
#include "rv_csr.h"
#include "rv_machine.h"
#include "testcase.h"

#include <stdatomic.h>
#include <time.h>

#define CLINT_BASE RV_CLINT_BASE

// mtime reads a non-zero, advancing value after a short sleep.
TESTCASE(clint_mtime_advances, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    uint64_t t0 = 0, t1 = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, CLINT_BASE + 0xBFF8, &t0, 3, RV_ACCESS_LOAD));

    struct timespec req = {.tv_sec = 0, .tv_nsec = 1000000}; // 1 ms
    nanosleep(&req, nullptr);

    TEST_ASSERT(rv_access_phys(machine, cpu, CLINT_BASE + 0xBFF8, &t1, 3, RV_ACCESS_LOAD));
    TEST_ASSERT(t1 > t0);

    rv_clint_destroy(&clint);
})

// Writing a far-future mtimecmp and calling check immediately → MTIP not set.
TESTCASE(clint_timer_not_fired_yet, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    // deadline ~100 s in the future (10^9 ticks × 100 ns = 100 s)
    uint64_t future = rv_clint_mtime(&clint) + UINT64_C(1000000000);
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0x4000, &future, 3, RV_ACCESS_STORE
    ));

    rv_clint_check_timer(&clint, cpu);

    uint64_t irq = atomic_load(&cpu->clint_irq);
    TEST_ASSERT(!(irq & (UINT64_C(1) << RV_CLINT_MTIP_BIT)));

    rv_clint_destroy(&clint);
})

// Writing mtimecmp=0 (always in the past) and calling check → MTIP set.
TESTCASE(clint_timer_past_fires, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0x4000, &zero, 3, RV_ACCESS_STORE
    ));

    rv_clint_check_timer(&clint, cpu);

    uint64_t irq = atomic_load(&cpu->clint_irq);
    TEST_ASSERT(irq & (UINT64_C(1) << RV_CLINT_MTIP_BIT));

    rv_clint_destroy(&clint);
})

// After MTIP fires, writing a future mtimecmp clears MTIP and re-arms the timer;
// an immediate check must not re-fire.
TESTCASE(clint_timer_rearm, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    // Fire the timer.
    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0x4000, &zero, 3, RV_ACCESS_STORE
    ));
    rv_clint_check_timer(&clint, cpu);
    TEST_ASSERT(atomic_load(&cpu->clint_irq) & (UINT64_C(1) << RV_CLINT_MTIP_BIT));

    // Re-arm with a future deadline.
    uint64_t future = rv_clint_mtime(&clint) + UINT64_C(1000000000);
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0x4000, &future, 3, RV_ACCESS_STORE
    ));

    // MTIP must be cleared after writing a new mtimecmp.
    TEST_ASSERT(!(atomic_load(&cpu->clint_irq) & (UINT64_C(1) << RV_CLINT_MTIP_BIT)));
    // Armed flag must be set.
    TEST_ASSERT(atomic_load(&cpu->clint_timer_armed));

    // Immediate check must not re-fire (future deadline).
    rv_clint_check_timer(&clint, cpu);
    TEST_ASSERT(!(atomic_load(&cpu->clint_irq) & (UINT64_C(1) << RV_CLINT_MTIP_BIT)));

    rv_clint_destroy(&clint);
})

// MSIP write to hart 0 sets/clears only that hart's MSIP bit; hart 1 unaffected.
TESTCASE_NOMACHINE(clint_msip_hart0, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 2, 0x10000, 0x1000));

    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, &machine));
    TEST_ASSERT(rv_machine_add_mmio(&machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine.clint = &clint;

    struct rv_cpu *cpu0 = &machine.cpus[0];
    struct rv_cpu *cpu1 = &machine.cpus[1];

    // Set MSIP for hart 0.
    uint32_t one = 1;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, CLINT_BASE + 0x0000, &one, 2, RV_ACCESS_STORE));
    TEST_ASSERT(atomic_load(&cpu0->clint_irq) & (UINT64_C(1) << RV_CLINT_MSIP_BIT));
    TEST_ASSERT(!(atomic_load(&cpu1->clint_irq) & (UINT64_C(1) << RV_CLINT_MSIP_BIT)));

    // Clear MSIP for hart 0.
    uint32_t zero = 0;
    TEST_ASSERT(rv_access_phys(&machine, cpu0, CLINT_BASE + 0x0000, &zero, 2, RV_ACCESS_STORE));
    TEST_ASSERT(!(atomic_load(&cpu0->clint_irq) & (UINT64_C(1) << RV_CLINT_MSIP_BIT)));

    // Set MSIP for hart 1 — hart 0 must remain clear.
    TEST_ASSERT(rv_access_phys(&machine, cpu0, CLINT_BASE + 0x0004, &one, 2, RV_ACCESS_STORE));
    TEST_ASSERT(!(atomic_load(&cpu0->clint_irq) & (UINT64_C(1) << RV_CLINT_MSIP_BIT)));
    TEST_ASSERT(atomic_load(&cpu1->clint_irq) & (UINT64_C(1) << RV_CLINT_MSIP_BIT));

    rv_clint_destroy(&clint);
    rv_machine_destroy(&machine);
})

// MSIP write via MMIO is visible in firmware's mip CSR read (bit 3).
TESTCASE(clint_msip_visible_in_mip, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    // Assert MSIP.
    uint32_t one = 1;
    TEST_ASSERT(rv_access_phys(machine, cpu, CLINT_BASE + 0x0000, &one, 2, RV_ACCESS_STORE));

    uint64_t mip = 0;
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(mip & (UINT64_C(1) << RV_CLINT_MSIP_BIT));

    // Deassert MSIP.
    uint32_t zero = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, CLINT_BASE + 0x0000, &zero, 2, RV_ACCESS_STORE));
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(!(mip & (UINT64_C(1) << RV_CLINT_MSIP_BIT)));

    rv_clint_destroy(&clint);
})

// MTIP fires and is visible in firmware's mip CSR read (bit 7).
TESTCASE(clint_mtip_visible_in_mip, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    // Write mtimecmp=0 (always past) and check.
    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, CLINT_BASE + 0x4000, &zero, 3, RV_ACCESS_STORE));
    rv_clint_check_timer(&clint, cpu);

    uint64_t mip = 0;
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(mip & (UINT64_C(1) << RV_CLINT_MTIP_BIT));

    rv_clint_destroy(&clint);
})

// Writing mtime via MMIO adjusts the epoch so reads return approximately the written value.
TESTCASE(clint_mtime_write_adjusts, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE)));
    machine->clint = &clint;

    uint64_t target = 1000;
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0xBFF8, &target, 3, RV_ACCESS_STORE
    ));

    uint64_t readback = 0;
    TEST_ASSERT(rv_access_phys(
        machine, cpu, CLINT_BASE + 0xBFF8, &readback, 3, RV_ACCESS_LOAD
    ));

    // Allow a small delta for the two clock_gettime calls between write and read.
    TEST_ASSERT(readback >= target);
    TEST_ASSERT(readback < target + 1000); // < 100 µs drift

    rv_clint_destroy(&clint);
})
