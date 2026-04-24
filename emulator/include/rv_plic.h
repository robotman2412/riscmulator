
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pthread.h>

struct rv_machine;

// Number of interrupt sources supported (source 0 is reserved and never pending).
// Valid source identifiers are 1..RV_PLIC_NUM_SOURCES.
#define RV_PLIC_NUM_SOURCES 64
// 32-bit words required to hold one bit per source (matches the PLIC register layout).
#define RV_PLIC_REG_WORDS   ((RV_PLIC_NUM_SOURCES + 31) / 32)

// Bit positions in cpu->plic_irq (mirrors mip layout).
#define RV_PLIC_MEIP_BIT 11 // machine external interrupt pending
#define RV_PLIC_SEIP_BIT 9  // supervisor external interrupt pending

// Per-context PLIC state.  Context index = hart * 2 + mode  (mode: 0=M, 1=S).
struct rv_plic_context {
    uint32_t threshold;
    uint32_t enable[RV_PLIC_REG_WORDS];
};

// PLIC device state.  Zero-initialise before calling rv_plic_init.
struct rv_plic {
    // Source priority registers; index 0 is unused (source 0 reserved).
    uint32_t                priority[RV_PLIC_NUM_SOURCES + 1];
    // Interrupt pending bits; bit N corresponds to source N.
    uint32_t                pending[RV_PLIC_REG_WORDS];
    // Per-context state; num_contexts = machine->cpu_count * 2.
    struct rv_plic_context *contexts;
    size_t                  num_contexts;
    // Back-reference for updating hart interrupt lines after state changes.
    struct rv_machine      *machine;
    pthread_mutex_t         lock;
};

// Initialise the PLIC; num_contexts is derived from machine->cpu_count.
// The caller must zero-initialise *plic before this call.
bool rv_plic_init(struct rv_plic *plic, struct rv_machine *machine);

// Free resources allocated by rv_plic_init.  Does not free the struct itself.
void rv_plic_destroy(struct rv_plic *plic);

// Assert a pending interrupt from a device (source 1..RV_PLIC_NUM_SOURCES).
// Thread-safe; may be called from any thread at any time.
void rv_plic_set_pending(struct rv_plic *plic, uint32_t source);

// Build an rv_mmio_region for registering this PLIC with rv_machine_add_mmio.
// The region starts at `base` and spans 0x400000 bytes (standard PLIC size).
struct rv_mmio_region rv_plic_mmio_region(struct rv_plic *plic, uint64_t base);
