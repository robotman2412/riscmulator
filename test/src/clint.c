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
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    uint64_t t0 = 0, t1 = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0xBFF8,
        &t0,
        3,
        RV_ACCESS_LOAD,
        false
    ) == RV_MEM_OK);

    struct timespec req = {.tv_sec = 0, .tv_nsec = 1000000}; // 1 ms
    nanosleep(&req, nullptr);

    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0xBFF8,
        &t1,
        3,
        RV_ACCESS_LOAD,
        false
    ) == RV_MEM_OK);
    TEST_ASSERT(t1 > t0);

    rv_clint_destroy(&clint);
})

// Writing a far-future mtimecmp and calling check immediately → MTIP not set.
TESTCASE(clint_timer_not_fired_yet, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    // deadline ~100 s in the future (10^9 ticks × 100 ns = 100 s)
    uint64_t future = rv_clint_mtime(&clint) + UINT64_C(1000000000);
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x4000,
        &future,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);

    rv_clint_check_timer(&clint, cpu);

    uint64_t irq = atomic_load(&cpu->irq_pending);
    TEST_ASSERT(!(irq & (UINT64_C(1) << RV_CLINT_MTIP_BIT)));

    rv_clint_destroy(&clint);
})

// Writing mtimecmp=0 (always in the past) and calling check → MTIP set.
TESTCASE(clint_timer_past_fires, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x4000,
        &zero,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);

    rv_clint_check_timer(&clint, cpu);

    uint64_t irq = atomic_load(&cpu->irq_pending);
    TEST_ASSERT(irq & (UINT64_C(1) << RV_CLINT_MTIP_BIT));

    rv_clint_destroy(&clint);
})

// After MTIP fires, writing a future mtimecmp clears MTIP and re-arms the
// timer; an immediate check must not re-fire.
TESTCASE(clint_timer_rearm, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    // Fire the timer.
    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x4000,
        &zero,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    rv_clint_check_timer(&clint, cpu);
    TEST_ASSERT(
        atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_CLINT_MTIP_BIT)
    );

    // Re-arm with a future deadline.
    uint64_t future = rv_clint_mtime(&clint) + UINT64_C(1000000000);
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x4000,
        &future,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);

    // MTIP must be cleared after writing a new mtimecmp.
    TEST_ASSERT(
        !(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_CLINT_MTIP_BIT))
    );
    // Armed flag must be set.
    TEST_ASSERT(atomic_load(&cpu->clint_timer_armed));

    // Immediate check must not re-fire (future deadline).
    rv_clint_check_timer(&clint, cpu);
    TEST_ASSERT(
        !(atomic_load(&cpu->irq_pending) & (UINT64_C(1) << RV_CLINT_MTIP_BIT))
    );

    rv_clint_destroy(&clint);
})

// MSIP write to hart 0 sets/clears only that hart's MSIP bit; hart 1
// unaffected.
TESTCASE_NOMACHINE(clint_msip_hart0, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 2, 0x10000, 0x1000));

    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, &machine));
    TEST_ASSERT(
        rv_machine_add_mmio(&machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine.clint = &clint;

    struct rv_cpu *cpu0 = &machine.cpus[0];
    struct rv_cpu *cpu1 = &machine.cpus[1];

    // Set MSIP for hart 0.
    uint32_t one = 1;
    TEST_ASSERT(rv_access_phys(
        &machine,
        cpu0,
        CLINT_BASE + 0x0000,
        &one,
        2,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    TEST_ASSERT(
        atomic_load(&cpu0->irq_pending) & (UINT64_C(1) << RV_CLINT_MSIP_BIT)
    );
    TEST_ASSERT(
        !(atomic_load(&cpu1->irq_pending) & (UINT64_C(1) << RV_CLINT_MSIP_BIT))
    );

    // Clear MSIP for hart 0.
    uint32_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        &machine,
        cpu0,
        CLINT_BASE + 0x0000,
        &zero,
        2,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    TEST_ASSERT(
        !(atomic_load(&cpu0->irq_pending) & (UINT64_C(1) << RV_CLINT_MSIP_BIT))
    );

    // Set MSIP for hart 1 — hart 0 must remain clear.
    TEST_ASSERT(rv_access_phys(
        &machine,
        cpu0,
        CLINT_BASE + 0x0004,
        &one,
        2,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    TEST_ASSERT(
        !(atomic_load(&cpu0->irq_pending) & (UINT64_C(1) << RV_CLINT_MSIP_BIT))
    );
    TEST_ASSERT(
        atomic_load(&cpu1->irq_pending) & (UINT64_C(1) << RV_CLINT_MSIP_BIT)
    );

    rv_clint_destroy(&clint);
    rv_machine_destroy(&machine);
})

// MSIP write via MMIO is visible in firmware's mip CSR read (bit 3).
TESTCASE(clint_msip_visible_in_mip, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    // Assert MSIP.
    uint32_t one = 1;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x0000,
        &one,
        2,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);

    uint64_t mip = 0;
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(mip & (UINT64_C(1) << RV_CLINT_MSIP_BIT));

    // Deassert MSIP.
    uint32_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x0000,
        &zero,
        2,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(!(mip & (UINT64_C(1) << RV_CLINT_MSIP_BIT)));

    rv_clint_destroy(&clint);
})

// MTIP fires and is visible in firmware's mip CSR read (bit 7).
TESTCASE(clint_mtip_visible_in_mip, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    // Write mtimecmp=0 (always past) and check.
    uint64_t zero = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0x4000,
        &zero,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);
    rv_clint_check_timer(&clint, cpu);

    uint64_t mip = 0;
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_mip, &mip));
    TEST_ASSERT(mip & (UINT64_C(1) << RV_CLINT_MTIP_BIT));

    rv_clint_destroy(&clint);
})

// mip write only touches SIP_WMASK bits; hardware-owned bits (MSIP=3, MTIP=7,
// MEIP=11) are ignored.
TESTCASE_NOMACHINE(mip_write_mask_hw_bits_ignored, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 1, 0x80000000, 0x1000));
    struct rv_cpu *cpu = &machine.cpus[0];

    // Pre-set a known MSIP (bit 3) via irq_pending directly.
    atomic_store(&cpu->irq_pending, UINT64_C(0));

    // Write all bits via mip CSR — hardware bits must not stick.
    TEST_ASSERT(rv_csr_write(cpu, RV_CSR_mip, UINT64_MAX));

    uint64_t ip = atomic_load(&cpu->irq_pending);
    TEST_ASSERT(!(ip & (UINT64_C(1) << 3)));  // MSIP not set
    TEST_ASSERT(!(ip & (UINT64_C(1) << 7)));  // MTIP not set
    TEST_ASSERT(!(ip & (UINT64_C(1) << 11))); // MEIP not set
    // SIP_WMASK bits must be set.
    TEST_ASSERT(ip & (UINT64_C(1) << 1)); // SSIP set
    TEST_ASSERT(ip & (UINT64_C(1) << 5)); // STIP set
    TEST_ASSERT(ip & (UINT64_C(1) << 9)); // SEIP set

    rv_machine_destroy(&machine);
})

// mip write clears SIP_WMASK bits when wdata=0; hardware bits in irq_pending
// untouched.
TESTCASE_NOMACHINE(mip_write_clears_sip_bits, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 1, 0x80000000, 0x1000));
    struct rv_cpu *cpu = &machine.cpus[0];

    // Pre-set all bits (simulating hardware having set MSIP/MTIP/MEIP).
    atomic_store(&cpu->irq_pending, UINT64_MAX);

    // Write zero via mip CSR — only SIP_WMASK bits should be cleared.
    TEST_ASSERT(rv_csr_write(cpu, RV_CSR_mip, UINT64_C(0)));

    uint64_t ip = atomic_load(&cpu->irq_pending);
    TEST_ASSERT(!(ip & (UINT64_C(1) << 1))); // SSIP cleared
    TEST_ASSERT(!(ip & (UINT64_C(1) << 5))); // STIP cleared
    TEST_ASSERT(!(ip & (UINT64_C(1) << 9))); // SEIP cleared
    // Hardware bits must be untouched.
    TEST_ASSERT(ip & (UINT64_C(1) << 3));  // MSIP still set
    TEST_ASSERT(ip & (UINT64_C(1) << 7));  // MTIP still set
    TEST_ASSERT(ip & (UINT64_C(1) << 11)); // MEIP still set

    rv_machine_destroy(&machine);
})

// sip write is masked by mideleg & SIP_WMASK; hardware-owned bits never
// affected.
TESTCASE_NOMACHINE(sip_write_mask, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 1, 0x80000000, 0x1000));
    struct rv_cpu *cpu = &machine.cpus[0];
    cpu->privilege     = 1; // S-mode
    // Delegate all software-writable interrupt bits to S-mode.
    cpu->csr.mideleg   = RV_SIP_WMASK;

    atomic_store(&cpu->irq_pending, UINT64_C(0));

    TEST_ASSERT(rv_csr_write(cpu, RV_CSR_sip, UINT64_MAX));

    uint64_t ip = atomic_load(&cpu->irq_pending);
    TEST_ASSERT(!(ip & (UINT64_C(1) << 3)));  // MSIP not set
    TEST_ASSERT(!(ip & (UINT64_C(1) << 7)));  // MTIP not set
    TEST_ASSERT(!(ip & (UINT64_C(1) << 11))); // MEIP not set
    TEST_ASSERT(ip & (UINT64_C(1) << 1));     // SSIP set
    TEST_ASSERT(ip & (UINT64_C(1) << 5));     // STIP set
    TEST_ASSERT(ip & (UINT64_C(1) << 9));     // SEIP set

    rv_machine_destroy(&machine);
})

// sip read shows only delegated bits (mideleg masking); non-delegated bits
// invisible.
TESTCASE_NOMACHINE(sip_read_reflects_irq_pending, {
    struct rv_machine machine = {0};
    TEST_ASSERT(rv_machine_init(&machine, 1, 0x80000000, 0x1000));
    struct rv_cpu *cpu = &machine.cpus[0];
    cpu->privilege     = 1; // S-mode
    // Delegate SSIP to S-mode; leave MSIP (bit 3) non-delegated.
    cpu->csr.mideleg   = UINT64_C(1) << RV_MIP_SSIP_BIT;

    // Fire both SSIP and MSIP directly.
    atomic_store(
        &cpu->irq_pending,
        (UINT64_C(1) << RV_MIP_SSIP_BIT) | (UINT64_C(1) << RV_MIP_MSIP_BIT)
    );

    uint64_t sip = 0;
    TEST_ASSERT(rv_csr_read(cpu, RV_CSR_sip, &sip));
    TEST_ASSERT(sip & (UINT64_C(1) << RV_MIP_SSIP_BIT)); // delegated: visible
    TEST_ASSERT(
        !(sip & (UINT64_C(1) << RV_MIP_MSIP_BIT))
    ); // not delegated: hidden

    rv_machine_destroy(&machine);
})

// Writing mtime via MMIO adjusts the epoch so reads return approximately the
// written value.
TESTCASE(clint_mtime_write_adjusts, {
    struct rv_clint clint = {0};
    TEST_ASSERT(rv_clint_init(&clint, machine));
    TEST_ASSERT(
        rv_machine_add_mmio(machine, rv_clint_mmio_region(&clint, CLINT_BASE))
    );
    machine->clint = &clint;

    uint64_t target = 1000;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0xBFF8,
        &target,
        3,
        RV_ACCESS_STORE,
        false
    ) == RV_MEM_OK);

    uint64_t readback = 0;
    TEST_ASSERT(rv_access_phys(
        machine,
        cpu,
        CLINT_BASE + 0xBFF8,
        &readback,
        3,
        RV_ACCESS_LOAD,
        false
    ) == RV_MEM_OK);

    // Allow a small delta for the two clock_gettime calls between write and
    // read.
    TEST_ASSERT(readback >= target);
    TEST_ASSERT(readback < target + 1000); // < 100 µs drift

    rv_clint_destroy(&clint);
})
