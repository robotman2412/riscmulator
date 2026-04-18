
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_machine.h"
#include "rv_cpu.h"

#include <stdlib.h>
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
    struct rv_cpu         *cpus  = calloc(cpu_count, sizeof(struct rv_cpu));
    struct rv_reservation *resv  = calloc(cpu_count, sizeof(struct rv_reservation));
    uint8_t               *ram   = calloc(1, ram_size);

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
