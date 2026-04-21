
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_machine.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_pmp.h"
#include "rv_privileged.h"

#include <stdint.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>

struct cpu_thread_arg {
    struct rv_machine *machine;
    struct rv_cpu     *cpu;
};

static void *cpu_thread(void *arg) {
    struct cpu_thread_arg *a = arg;
    while (!a->cpu->halted) {
        rv_step_insn(a->machine, a->cpu);
    }
    return nullptr;
}

bool rv_machine_init(
    struct rv_machine *machine,
    size_t             cpu_count,
    uint64_t           ram_start,
    size_t             ram_size
) {
    struct rv_cpu         *cpus = calloc(cpu_count, sizeof(struct rv_cpu));
    struct rv_reservation *resv =
        calloc(cpu_count, sizeof(struct rv_reservation));
    uint8_t *ram = calloc(1, ram_size);

    if (!cpus || !resv || !ram) {
        free(cpus);
        free(resv);
        free(ram);
        return false;
    }

    for (size_t i = 0; i < cpu_count; i++) {
        cpus[i].csr.mhartid = i;
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

void rv_machine_run(struct rv_machine *machine) {
    pthread_t *threads = malloc(machine->cpu_count * sizeof(pthread_t));
    struct cpu_thread_arg *args =
        malloc(machine->cpu_count * sizeof(struct cpu_thread_arg));

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

void rv_machine_destroy(struct rv_machine *machine) {
    pthread_mutex_destroy(&machine->atomic_lock);
    free(machine->cpus);
    free(machine->reservations);
    free(machine->ram);
    machine->cpus         = nullptr;
    machine->reservations = nullptr;
    machine->ram          = nullptr;
    machine->cpu_count    = 0;
    machine->ram_start    = 0;
    machine->ram_end      = 0;
}

// Implementation of misaligned accesses.
static bool misaligned_access(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    uint64_t          *data,
    size_t             size,
    enum rv_access     mode
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
                memcpy(data, ptr, part);
            } else if (data) {
                memcpy(ptr, data, part);
            }
            addr += part;
            size -= part;

        } else { // TODO: Partial MMIO access.
            // Invalid access.
            enum rv_cause cause;
            switch (mode) {
                case RV_ACCESS_INSN: cause = RV_CAUSE_IACCESS; break;
                case RV_ACCESS_LOAD: cause = RV_CAUSE_LACCESS; break;
                case RV_ACCESS_AMO:
                case RV_ACCESS_STORE: cause = RV_CAUSE_SACCESS; break;
            }
            rv_do_trap(
                machine,
                cpu,
                (struct rv_trap){
                    .cause = cause,
                    .epc   = cpu->epc,
                    .tval  = addr,
                }
            );
            return false;
        }
    }

    return true;
}

static inline bool do_pmp_check(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    size_t             size,
    enum rv_access     mode
) {
    uint8_t perm = rv_pmp_check(machine, cpu, addr, size, cpu->privilege == 3);

    bool          ok;
    enum rv_cause cause;
    switch (mode) {
        case RV_ACCESS_INSN:
            cause = RV_CAUSE_IACCESS;
            ok    = perm & (1 << RV_PMPCFG_X_BIT);
            break;
        case RV_ACCESS_LOAD:
            cause = RV_CAUSE_LACCESS;
            ok    = perm & (1 << RV_PMPCFG_R_BIT);
            break;
        case RV_ACCESS_AMO:
        case RV_ACCESS_STORE:
            cause = RV_CAUSE_SACCESS;
            ok    = perm & (1 << RV_PMPCFG_W_BIT);
            break;
    }
    if (!ok) {
        rv_do_trap(
            machine,
            cpu,
            (struct rv_trap){
                .cause = cause,
                .epc   = cpu->epc,
                .tval  = addr,
            }
        );
        return false;
    }

    return true;
}

// Partial access to physical memory (e.g. spanning virtual page boundary).
bool rv_access_phys_partial(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    void              *data,
    size_t             size,
    enum rv_access     mode
) {
    return do_pmp_check(machine, cpu, addr, size, mode) &&
           // The first checks the access permissions,
           misaligned_access(machine, cpu, addr, nullptr, size, mode) &&
           // the second actually commits to the access.
           misaligned_access(machine, cpu, addr, data, size, mode);
}

// Access physical memory, optimizing for aligned access.
bool rv_access_phys(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    void              *data,
    uint8_t            size_exp,
    enum rv_access     mode
) {
    uint64_t size = UINT64_C(1) << size_exp;

    if (!do_pmp_check(machine, cpu, addr, size, mode)) {
        return false;
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
        return true;
    }

    // Break up into aligned accesses if needed.
    if (addr % size) {
        // The first checks the access permissions,
        // the second actually commits to the access.
        return misaligned_access(machine, cpu, addr, nullptr, size, mode) &&
               misaligned_access(machine, cpu, addr, data, size, mode);
    }

    // Invalid access.
    enum rv_cause cause;
    switch (mode) {
        case RV_ACCESS_INSN: cause = RV_CAUSE_IACCESS; break;
        case RV_ACCESS_LOAD: cause = RV_CAUSE_LACCESS; break;
        case RV_ACCESS_AMO:
        case RV_ACCESS_STORE: cause = RV_CAUSE_SACCESS; break;
    }
    rv_do_trap(
        machine,
        cpu,
        (struct rv_trap){
            .epc   = cpu->epc,
            .tval  = addr,
            .cause = cause,
        }
    );
    return false;
}
