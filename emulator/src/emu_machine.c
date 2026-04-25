
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "emu_machine.h"

#include "cpu/rv_cpu.h"
#include "cpu/rv_csr.h"
#include "cpu/rv_pmp.h"
#include "cpu/rv_privileged.h"
#include "device/rv_clint.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct cpu_thread_arg {
    struct emu_machine *machine;
    struct rv_cpu      *cpu;
};

static void *cpu_thread(void *arg) {
    struct cpu_thread_arg *a = arg;
    while (!a->cpu->halted) {
        rv_step_insn(a->machine, a->cpu);
        if (atomic_load_explicit(&a->cpu->clint_timer_armed, memory_order_relaxed))
            rv_clint_check_timer(a->machine->clint, a->cpu);
    }
    return nullptr;
}

bool emu_machine_init(struct emu_machine *machine, size_t cpu_count, uint64_t ram_start, size_t ram_size) {
    struct rv_cpu         *cpus = calloc(cpu_count, sizeof(struct rv_cpu));
    struct rv_reservation *resv = calloc(cpu_count, sizeof(struct rv_reservation));
    uint8_t               *ram  = calloc(1, ram_size);

    if (!cpus || !resv || !ram) {
        free(cpus);
        free(resv);
        free(ram);
        return false;
    }

    for (size_t i = 0; i < cpu_count; i++) {
        cpus[i].csr.mhartid = i;
        cpus[i].privilege   = 3; // CPUs reset into M-mode per RISC-V spec.
        rv_sync_mem_privilege(&cpus[i]);
    }

    machine->cpus         = cpus;
    machine->cpu_count    = cpu_count;
    machine->reservations = resv;
    machine->ram          = ram;
    machine->ram_start    = ram_start;
    machine->ram_end      = ram_start + ram_size;
    pthread_mutex_init(&machine->atomic_lock, nullptr);
    return true;
}

void emu_machine_run(struct emu_machine *machine) {
    pthread_t             *threads = malloc(machine->cpu_count * sizeof(pthread_t));
    struct cpu_thread_arg *args    = malloc(machine->cpu_count * sizeof(struct cpu_thread_arg));

    for (size_t i = 0; i < machine->cpu_count; i++) {
        args[i].machine = machine;
        args[i].cpu     = &machine->cpus[i];
        pthread_create(&threads[i], nullptr, cpu_thread, &args[i]);
    }

    for (size_t i = 0; i < machine->cpu_count; i++) {
        pthread_join(threads[i], nullptr);
    }

    free(threads);
    free(args);
}

void emu_machine_destroy(struct emu_machine *machine) {
    pthread_mutex_destroy(&machine->atomic_lock);
    free(machine->cpus);
    free(machine->reservations);
    free(machine->ram);
    free(machine->mmio);
    machine->cpus         = nullptr;
    machine->reservations = nullptr;
    machine->ram          = nullptr;
    machine->mmio         = nullptr;
    machine->cpu_count    = 0;
    machine->mmio_count   = 0;
    machine->ram_start    = 0;
    machine->ram_end      = 0;
}

bool emu_machine_add_mmio(struct emu_machine *machine, struct emu_mmio_region region) {
    struct emu_mmio_region *arr = realloc(machine->mmio, (machine->mmio_count + 1) * sizeof(struct emu_mmio_region));
    if (!arr)
        return false;
    machine->mmio                        = arr;
    machine->mmio[machine->mmio_count++] = region;
    return true;
}

// Dispatch a single MMIO access to a known region.
// data=nullptr is a no-op (permission-check pass); returns RV_MEM_OK.
static enum rv_mem_result mmio_dispatch(
    struct emu_machine     *machine,
    struct rv_cpu          *cpu,
    struct emu_mmio_region *r,
    uint64_t                addr,
    void                   *data,
    uint8_t                 size,
    enum rv_access          mode
) {
    (void)machine;
    (void)cpu;
    if (!data)
        return RV_MEM_OK;

    bool ok;
    if (mode == RV_ACCESS_STORE) {
        uint64_t val = 0;
        memcpy(&val, data, size);
        ok = r->write(r->device, addr - r->base, size, val);
    } else {
        uint64_t val = 0;
        ok           = r->read(r->device, addr - r->base, size, &val);
        if (ok)
            memcpy(data, &val, size);
    }

    return ok ? RV_MEM_OK : RV_MEM_ACCESS_FAULT;
}

// Implementation of misaligned accesses.
static enum rv_mem_result misaligned_access(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t addr, uint64_t *data, size_t size, enum rv_access mode
) {
    // Loop (partial) accesses until done.
    while (size) {
        if (addr >= machine->ram_start && addr + 1 <= machine->ram_end) {
            // Partial RAM access.
            uint64_t part = size;
            if (addr + part > machine->ram_end) {
                part = machine->ram_end - addr;
            }

            void *ptr = machine->ram + addr - machine->ram_start;
            if (data && mode == RV_ACCESS_STORE) {
                memcpy(ptr, data, part);
            } else if (data) {
                memcpy(data, ptr, part);
            }
            addr += part;
            size -= part;

        } else {
            // Not in RAM — try MMIO.
            struct emu_mmio_region *r = nullptr;
            for (size_t i = 0; i < machine->mmio_count; i++) {
                if (addr >= machine->mmio[i].base && addr < machine->mmio[i].base + machine->mmio[i].size) {
                    r = &machine->mmio[i];
                    break;
                }
            }
            if (!r) {
                return RV_MEM_ACCESS_FAULT;
            }
            uint64_t part = size;
            if (addr + part > r->base + r->size) {
                part = r->base + r->size - addr;
            }
            enum rv_mem_result mr = mmio_dispatch(machine, cpu, r, addr, data, (uint8_t)part, mode);
            if (mr != RV_MEM_OK) {
                return mr;
            }
            addr += part;
            size -= part;
        }
    }

    return RV_MEM_OK;
}

static inline enum rv_mem_result
    do_pmp_check(struct emu_machine *machine, struct rv_cpu *cpu, uint64_t addr, size_t size, enum rv_access mode) {
    uint8_t perm = rv_pmp_check(machine, cpu, addr, size, cpu->privilege == 3);

    bool ok;
    switch (mode) {
        case RV_ACCESS_INSN: ok = perm & (1 << RV_PMPCFG_X_BIT); break;
        case RV_ACCESS_LOAD: ok = perm & (1 << RV_PMPCFG_R_BIT); break;
        case RV_ACCESS_AMO:
        case RV_ACCESS_STORE: ok = perm & (1 << RV_PMPCFG_W_BIT); break;
    }
    return ok ? RV_MEM_OK : RV_MEM_ACCESS_FAULT;
}

// Partial access to physical memory (e.g. spanning virtual page boundary).
enum rv_mem_result rv_access_phys_partial(
    struct emu_machine *machine,
    struct rv_cpu      *cpu,
    uint64_t            addr,
    void               *data,
    size_t              size,
    enum rv_access      mode,
    bool                ignore_pmp
) {
    if (!ignore_pmp) {
        enum rv_mem_result r = do_pmp_check(machine, cpu, addr, size, mode);
        if (r != RV_MEM_OK)
            return r;
    }
    // The first pass checks access permissions, the second commits.
    enum rv_mem_result r = misaligned_access(machine, cpu, addr, nullptr, size, mode);
    if (r != RV_MEM_OK)
        return r;
    return misaligned_access(machine, cpu, addr, data, size, mode);
}

// Access physical memory, optimizing for aligned access.
enum rv_mem_result rv_access_phys(
    struct emu_machine *machine,
    struct rv_cpu      *cpu,
    uint64_t            addr,
    void               *data,
    uint8_t             size_exp,
    enum rv_access      mode,
    bool                ignore_pmp
) {
    uint64_t size = UINT64_C(1) << size_exp;

    if (!ignore_pmp) {
        enum rv_mem_result r = do_pmp_check(machine, cpu, addr, size, mode);
        if (r != RV_MEM_OK)
            return r;
    }

    // Aligned RAM access fast path.
    if (addr >= machine->ram_start && addr + size <= machine->ram_end) {
        void *ptr = machine->ram + addr - machine->ram_start;
        if (mode == RV_ACCESS_STORE) {
            switch (size_exp) {
                case 0: *(uint8_t *)ptr = *(uint8_t *)data; break;
                case 1: *(uint16_t *)ptr = *(uint16_t *)data; break;
                case 2: *(uint32_t *)ptr = *(uint32_t *)data; break;
                case 3: *(uint64_t *)ptr = *(uint64_t *)data; break;
            }
        } else {
            switch (size_exp) {
                case 0: *(uint8_t *)data = *(uint8_t *)ptr; break;
                case 1: *(uint16_t *)data = *(uint16_t *)ptr; break;
                case 2: *(uint32_t *)data = *(uint32_t *)ptr; break;
                case 3: *(uint64_t *)data = *(uint64_t *)ptr; break;
            }
        }
        return RV_MEM_OK;
    }

    // Break up into aligned accesses if needed.
    if (addr % size) {
        // The first pass checks permissions, the second commits.
        enum rv_mem_result r = misaligned_access(machine, cpu, addr, nullptr, size, mode);
        if (r != RV_MEM_OK)
            return r;
        return misaligned_access(machine, cpu, addr, data, size, mode);
    }

    // Aligned MMIO dispatch.
    for (size_t i = 0; i < machine->mmio_count; i++) {
        struct emu_mmio_region *r = &machine->mmio[i];
        if (addr >= r->base && addr + size <= r->base + r->size) {
            return mmio_dispatch(machine, cpu, r, addr, data, (uint8_t)size, mode);
        }
    }

    return RV_MEM_ACCESS_FAULT;
}
