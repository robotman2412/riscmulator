
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_clint.h"

#include "rv_cpu.h"
#include "rv_machine.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

// Tick frequency: 10 MHz → 100 ns per tick.
#define CLINT_NS_PER_TICK UINT64_C(100)

// MSIP register block: offset 0x0000, 4 bytes per hart.
#define CLINT_MSIP_BASE  UINT64_C(0x0000)
#define CLINT_MSIP_STRIDE UINT64_C(4)
// mtimecmp register block: offset 0x4000, 8 bytes per hart.
#define CLINT_MTIMECMP_BASE   UINT64_C(0x4000)
#define CLINT_MTIMECMP_STRIDE UINT64_C(8)
// mtime register: offset 0xBFF8, 8 bytes.
#define CLINT_MTIME_OFFSET UINT64_C(0xBFF8)

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

bool rv_clint_init(struct rv_clint *clint, struct rv_machine *machine) {
    size_t n = machine->cpu_count;

    uint64_t         *mtimecmp   = calloc(n, sizeof(uint64_t));
    _Atomic uint64_t *deadline   = calloc(n, sizeof(_Atomic uint64_t));
    if (!mtimecmp || !deadline) {
        free(mtimecmp);
        free(deadline);
        return false;
    }
    if (pthread_mutex_init(&clint->lock, nullptr) != 0) {
        free(mtimecmp);
        free(deadline);
        return false;
    }

    // Deadlines start at UINT64_MAX so an unarmed check never fires.
    for (size_t i = 0; i < n; i++) {
        atomic_init(&deadline[i], UINT64_MAX);
    }

    atomic_init(&clint->epoch_ns, now_ns());
    clint->mtimecmp  = mtimecmp;
    clint->deadline_ns = deadline;
    clint->cpu_count = n;
    clint->machine   = machine;
    return true;
}

void rv_clint_destroy(struct rv_clint *clint) {
    pthread_mutex_destroy(&clint->lock);
    free(clint->mtimecmp);
    free(clint->deadline_ns);
    clint->mtimecmp    = nullptr;
    clint->deadline_ns = nullptr;
    clint->cpu_count   = 0;
    clint->machine     = nullptr;
}

uint64_t rv_clint_mtime(struct rv_clint *clint) {
    uint64_t epoch = atomic_load_explicit(&clint->epoch_ns, memory_order_relaxed);
    return (now_ns() - epoch) / CLINT_NS_PER_TICK;
}

void rv_clint_set_mtimecmp(struct rv_clint *clint, uint64_t hart, uint64_t value) {
    struct rv_cpu *cpu = &clint->machine->cpus[hart];

    // Clear MTIP; the new deadline will re-fire immediately if already past.
    atomic_fetch_and_explicit(
        &cpu->irq_pending, ~(UINT64_C(1) << RV_CLINT_MTIP_BIT), memory_order_relaxed
    );

    pthread_mutex_lock(&clint->lock);
    clint->mtimecmp[hart]  = value;
    uint64_t epoch         = atomic_load_explicit(&clint->epoch_ns, memory_order_relaxed);
    uint64_t deadline      = epoch + value * CLINT_NS_PER_TICK;
    pthread_mutex_unlock(&clint->lock);

    // deadline_ns written relaxed before the release on clint_timer_armed;
    // the reader acquires on clint_timer_armed before reading deadline_ns.
    atomic_store_explicit(&clint->deadline_ns[hart], deadline, memory_order_relaxed);
    atomic_store_explicit(&cpu->clint_timer_armed, true, memory_order_release);
}

void rv_clint_check_timer(struct rv_clint *clint, struct rv_cpu *cpu) {
    uint64_t hart     = cpu->csr.mhartid;
    uint64_t deadline = atomic_load_explicit(
        &clint->deadline_ns[hart], memory_order_relaxed
    );
    if (now_ns() >= deadline) {
        atomic_fetch_or_explicit(
            &cpu->irq_pending, UINT64_C(1) << RV_CLINT_MTIP_BIT, memory_order_relaxed
        );
        atomic_store_explicit(&cpu->clint_timer_armed, false, memory_order_relaxed);
    }
}

// ---- MMIO callbacks ----

static bool clint_read(void *dev, uint64_t offset, uint8_t size, uint64_t *out) {
    struct rv_clint *clint = dev;

    // mtime (8-byte read only at 0xBFF8).
    if (offset == CLINT_MTIME_OFFSET && size == 8) {
        *out = rv_clint_mtime(clint);
        return true;
    }

    // mtimecmp[hart] (8-byte).
    if (offset >= CLINT_MTIMECMP_BASE && size == 8) {
        uint64_t idx = (offset - CLINT_MTIMECMP_BASE) / CLINT_MTIMECMP_STRIDE;
        if (idx < clint->cpu_count &&
            (offset - CLINT_MTIMECMP_BASE) % CLINT_MTIMECMP_STRIDE == 0) {
            pthread_mutex_lock(&clint->lock);
            *out = clint->mtimecmp[idx];
            pthread_mutex_unlock(&clint->lock);
            return true;
        }
    }

    // MSIP[hart] (4-byte; offset < CLINT_MTIMECMP_BASE).
    if (offset < CLINT_MTIMECMP_BASE && size == 4) {
        uint64_t idx = offset / CLINT_MSIP_STRIDE;
        if (idx < clint->cpu_count && offset % CLINT_MSIP_STRIDE == 0) {
            struct rv_cpu *cpu = &clint->machine->cpus[idx];
            uint64_t irq = atomic_load_explicit(&cpu->irq_pending, memory_order_relaxed);
            *out = (irq >> RV_CLINT_MSIP_BIT) & 1u;
            return true;
        }
    }

    return false;
}

static bool clint_write(void *dev, uint64_t offset, uint8_t size, uint64_t value) {
    struct rv_clint *clint = dev;

    // mtime (8-byte write — adjusts epoch so mtime reads back the written value).
    if (offset == CLINT_MTIME_OFFSET && size == 8) {
        uint64_t n       = now_ns();
        uint64_t new_epoch = n - value * CLINT_NS_PER_TICK;
        atomic_store_explicit(&clint->epoch_ns, new_epoch, memory_order_relaxed);
        pthread_mutex_lock(&clint->lock);
        for (size_t i = 0; i < clint->cpu_count; i++) {
            uint64_t d = new_epoch + clint->mtimecmp[i] * CLINT_NS_PER_TICK;
            atomic_store_explicit(&clint->deadline_ns[i], d, memory_order_relaxed);
        }
        pthread_mutex_unlock(&clint->lock);
        return true;
    }

    // mtimecmp[hart] (8-byte).
    if (offset >= CLINT_MTIMECMP_BASE && size == 8) {
        uint64_t idx = (offset - CLINT_MTIMECMP_BASE) / CLINT_MTIMECMP_STRIDE;
        if (idx < clint->cpu_count &&
            (offset - CLINT_MTIMECMP_BASE) % CLINT_MTIMECMP_STRIDE == 0) {
            rv_clint_set_mtimecmp(clint, idx, value);
            return true;
        }
    }

    // MSIP[hart] (4-byte; only bit 0 is writable; offset < CLINT_MTIMECMP_BASE).
    if (offset < CLINT_MTIMECMP_BASE && size == 4) {
        uint64_t idx = offset / CLINT_MSIP_STRIDE;
        if (idx < clint->cpu_count && offset % CLINT_MSIP_STRIDE == 0) {
            struct rv_cpu *cpu = &clint->machine->cpus[idx];
            if (value & 1u) {
                atomic_fetch_or_explicit(
                    &cpu->irq_pending, UINT64_C(1) << RV_CLINT_MSIP_BIT,
                    memory_order_relaxed
                );
            } else {
                atomic_fetch_and_explicit(
                    &cpu->irq_pending, ~(UINT64_C(1) << RV_CLINT_MSIP_BIT),
                    memory_order_relaxed
                );
            }
            return true;
        }
    }

    return false;
}

struct rv_mmio_region rv_clint_mmio_region(struct rv_clint *clint, uint64_t base) {
    return (struct rv_mmio_region){
        .base   = base,
        .size   = RV_CLINT_SIZE,
        .device = clint,
        .read   = clint_read,
        .write  = clint_write,
    };
}
