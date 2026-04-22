// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_plic.h"

#include "rv_cpu.h"
#include "rv_machine.h"

#include <stdatomic.h>
#include <stdlib.h>

// Standard RISC-V PLIC memory region size.
#define PLIC_REGION_SIZE UINT64_C(0x400000)

// Region offsets within the PLIC address space.
#define PLIC_PRIORITY_BASE    UINT64_C(0x000000)
#define PLIC_PENDING_BASE     UINT64_C(0x001000)
#define PLIC_ENABLE_BASE      UINT64_C(0x002000)
#define PLIC_ENABLE_CTX_STRIDE UINT64_C(0x080)
#define PLIC_CTX_BASE         UINT64_C(0x200000)
#define PLIC_CTX_STRIDE       UINT64_C(0x1000)

// Offsets within each context block.
#define PLIC_CTX_OFF_THRESHOLD UINT32_C(0x000)
#define PLIC_CTX_OFF_CLAIM     UINT32_C(0x004)

// Update all harts' plic_irq fields to reflect the current PLIC state.
// Must be called while plic->lock is held.
static void plic_update_harts(struct rv_plic *plic) {
    for (size_t ctx = 0; ctx < plic->num_contexts; ctx++) {
        size_t hart = ctx / 2;
        int    mode = (int)(ctx % 2); // 0 = M-mode, 1 = S-mode
        if (hart >= plic->machine->cpu_count) continue;

        struct rv_plic_context *c       = &plic->contexts[ctx];
        bool                    has_irq = false;

        for (uint32_t src = 1; src <= RV_PLIC_NUM_SOURCES && !has_irq; src++) {
            uint32_t word    = src / 32;
            uint32_t bit     = src % 32;
            bool     pending = (plic->pending[word] >> bit) & 1u;
            bool     enabled = (c->enable[word] >> bit) & 1u;
            if (pending && enabled && plic->priority[src] > c->threshold) {
                has_irq = true;
            }
        }

        struct rv_cpu *cpu    = &plic->machine->cpus[hart];
        uint64_t       irqbit = mode == 0 ? (UINT64_C(1) << RV_PLIC_MEIP_BIT)
                                          : (UINT64_C(1) << RV_PLIC_SEIP_BIT);
        if (has_irq) {
            atomic_fetch_or(&cpu->plic_irq, irqbit);
        } else {
            atomic_fetch_and(&cpu->plic_irq, ~irqbit);
        }
    }
}

// Find and return the highest-priority claimable source for the given context,
// clear its pending bit, and update hart interrupt lines.
// Returns 0 if no claimable source exists.  Must be called with lock held.
static uint32_t plic_do_claim(struct rv_plic *plic, uint32_t ctx) {
    if (ctx >= (uint32_t)plic->num_contexts) return 0;

    struct rv_plic_context *c        = &plic->contexts[ctx];
    uint32_t                best_src = 0;
    uint32_t                best_pri = 0;

    for (uint32_t src = 1; src <= RV_PLIC_NUM_SOURCES; src++) {
        uint32_t word    = src / 32;
        uint32_t bit     = src % 32;
        bool     pending = (plic->pending[word] >> bit) & 1u;
        bool     enabled = (c->enable[word] >> bit) & 1u;
        if (!pending || !enabled) continue;
        if (plic->priority[src] <= c->threshold) continue;
        if (plic->priority[src] > best_pri) {
            best_pri = plic->priority[src];
            best_src = src;
        }
    }

    if (best_src) {
        plic->pending[best_src / 32] &= ~(1u << (best_src % 32));
        plic_update_harts(plic);
    }

    return best_src;
}

static bool plic_read(void *dev, uint64_t offset, uint8_t size, uint64_t *out) {
    struct rv_plic *plic = dev;
    *out = 0;
    if (size != 4) return false;

    pthread_mutex_lock(&plic->lock);

    if (offset < PLIC_PENDING_BASE) {
        // Source priority registers.
        uint32_t src = (uint32_t)(offset / 4);
        if (src >= 1 && src <= RV_PLIC_NUM_SOURCES)
            *out = plic->priority[src];

    } else if (offset < PLIC_ENABLE_BASE) {
        // Interrupt pending bits (read-only).
        uint32_t word = (uint32_t)((offset - PLIC_PENDING_BASE) / 4);
        if (word < RV_PLIC_REG_WORDS)
            *out = plic->pending[word];

    } else if (offset < PLIC_CTX_BASE) {
        // Per-context enable bits.
        uint64_t rel  = offset - PLIC_ENABLE_BASE;
        uint32_t ctx  = (uint32_t)(rel / PLIC_ENABLE_CTX_STRIDE);
        uint32_t word = (uint32_t)((rel % PLIC_ENABLE_CTX_STRIDE) / 4);
        if (ctx < (uint32_t)plic->num_contexts && word < RV_PLIC_REG_WORDS)
            *out = plic->contexts[ctx].enable[word];

    } else {
        // Per-context threshold / claim.
        uint64_t rel = offset - PLIC_CTX_BASE;
        uint32_t ctx = (uint32_t)(rel / PLIC_CTX_STRIDE);
        uint32_t reg = (uint32_t)(rel % PLIC_CTX_STRIDE);
        if (ctx < (uint32_t)plic->num_contexts) {
            if (reg == PLIC_CTX_OFF_THRESHOLD) {
                *out = plic->contexts[ctx].threshold;
            } else if (reg == PLIC_CTX_OFF_CLAIM) {
                *out = plic_do_claim(plic, ctx);
            }
        }
    }

    pthread_mutex_unlock(&plic->lock);
    return true;
}

static bool plic_write(void *dev, uint64_t offset, uint8_t size, uint64_t value) {
    struct rv_plic *plic = dev;
    if (size != 4) return false;

    uint32_t v32 = (uint32_t)value;

    pthread_mutex_lock(&plic->lock);

    if (offset < PLIC_PENDING_BASE) {
        // Source priority registers; source 0 is reserved and ignores writes.
        uint32_t src = (uint32_t)(offset / 4);
        if (src >= 1 && src <= RV_PLIC_NUM_SOURCES) {
            plic->priority[src] = v32;
            plic_update_harts(plic);
        }

    } else if (offset < PLIC_ENABLE_BASE) {
        // Pending bits are read-only; writes silently ignored.

    } else if (offset < PLIC_CTX_BASE) {
        // Per-context enable bits.
        uint64_t rel  = offset - PLIC_ENABLE_BASE;
        uint32_t ctx  = (uint32_t)(rel / PLIC_ENABLE_CTX_STRIDE);
        uint32_t word = (uint32_t)((rel % PLIC_ENABLE_CTX_STRIDE) / 4);
        if (ctx < (uint32_t)plic->num_contexts && word < RV_PLIC_REG_WORDS) {
            plic->contexts[ctx].enable[word] = v32;
            plic_update_harts(plic);
        }

    } else {
        // Per-context threshold / complete.
        uint64_t rel = offset - PLIC_CTX_BASE;
        uint32_t ctx = (uint32_t)(rel / PLIC_CTX_STRIDE);
        uint32_t reg = (uint32_t)(rel % PLIC_CTX_STRIDE);
        if (ctx < (uint32_t)plic->num_contexts) {
            if (reg == PLIC_CTX_OFF_THRESHOLD) {
                plic->contexts[ctx].threshold = v32;
                plic_update_harts(plic);
            } else if (reg == PLIC_CTX_OFF_CLAIM) {
                // Complete: pending was already cleared on claim; no state change.
                (void)v32;
            }
        }
    }

    pthread_mutex_unlock(&plic->lock);
    return true;
}

bool rv_plic_init(struct rv_plic *plic, struct rv_machine *machine) {
    plic->machine      = machine;
    plic->num_contexts = machine->cpu_count * 2;

    plic->contexts = calloc(plic->num_contexts, sizeof(*plic->contexts));
    if (!plic->contexts) return false;

    if (pthread_mutex_init(&plic->lock, nullptr) != 0) {
        free(plic->contexts);
        plic->contexts = nullptr;
        return false;
    }

    return true;
}

void rv_plic_destroy(struct rv_plic *plic) {
    pthread_mutex_destroy(&plic->lock);
    free(plic->contexts);
    plic->contexts     = nullptr;
    plic->num_contexts = 0;
}

void rv_plic_set_pending(struct rv_plic *plic, uint32_t source) {
    if (source == 0 || source > RV_PLIC_NUM_SOURCES) return;

    pthread_mutex_lock(&plic->lock);
    plic->pending[source / 32] |= (1u << (source % 32));
    plic_update_harts(plic);
    pthread_mutex_unlock(&plic->lock);
}

struct rv_mmio_region rv_plic_mmio_region(struct rv_plic *plic, uint64_t base) {
    return (struct rv_mmio_region){
        .base   = base,
        .size   = PLIC_REGION_SIZE,
        .device = plic,
        .read   = plic_read,
        .write  = plic_write,
    };
}
