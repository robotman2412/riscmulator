
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "emu_device.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct emu_machine;
struct rv_cpu;

// CLINT MMIO base address (QEMU virt layout).
#define RV_CLINT_BASE UINT64_C(0x02000000)
// CLINT MMIO region size.
#define RV_CLINT_SIZE UINT64_C(0x10000)

// Bit positions in cpu->clint_irq (mirrors mip layout).
#define RV_CLINT_MSIP_BIT 3
#define RV_CLINT_MTIP_BIT 7

// CLINT device state.  Zero-initialise before calling rv_clint_init.
struct rv_clint {
    // CLOCK_MONOTONIC nanoseconds corresponding to virtual mtime = 0.
    _Atomic uint64_t    epoch_ns;
    // Per-hart mtimecmp shadow register; updated under lock.
    uint64_t           *mtimecmp;
    // Per-hart absolute CLOCK_MONOTONIC deadline (epoch_ns + mtimecmp * 100).
    // Written under lock with relaxed ordering before arming clint_timer_armed
    // (release); read relaxed after the acquire on clint_timer_armed.
    _Atomic uint64_t   *deadline_ns;
    size_t              cpu_count;
    struct emu_machine *machine;
    pthread_mutex_t     lock;
};

// Initialise the CLINT; cpu_count is derived from machine->cpu_count.
// The caller must zero-initialise *clint before this call.
bool rv_clint_init(struct rv_clint *clint, struct emu_machine *machine);

// Free resources allocated by rv_clint_init.  Does not free the struct itself.
void rv_clint_destroy(struct rv_clint *clint);

// Return the current virtual mtime tick counter (10 MHz; 1 tick = 100 ns).
uint64_t rv_clint_mtime(struct rv_clint *clint);

// Write mtimecmp[hart]: clears MTIP on that hart, stores value, computes the
// absolute deadline, and arms cpu->clint_timer_armed.
// Also used by the SBI shim to set the timer without going through MMIO.
void rv_clint_set_mtimecmp(struct rv_clint *clint, uint64_t hart, uint64_t value);

// Check whether the timer deadline has passed for this CPU and, if so, set
// the MTIP bit in cpu->clint_irq and disarm the flag.
// Called from cpu_thread after each instruction, guarded by clint_timer_armed.
void rv_clint_check_timer(struct rv_clint *clint, struct rv_cpu *cpu);

// Build an emu_mmio_region for registering this CLINT with emu_machine_add_mmio.
struct emu_mmio_region rv_clint_mmio_region(struct rv_clint *clint, uint64_t base);
