
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_privileged.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pthread.h>

struct rv_machine;
struct rv_cpu;

// RAM access mode.
enum rv_access {
    // Instruction fetch access.
    RV_ACCESS_INSN,
    // Load access.
    RV_ACCESS_LOAD,
    // Store access.
    RV_ACCESS_STORE,
};

// Callback to run instead of the normal trap mechanism.
// If this returns true, the normal trap mechanism activates.
typedef bool (*rv_trap_fn_t)(
    void              *cookie,
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    struct rv_trap     trap
);

// Per-hart LR reservation entry; one per CPU indexed by mhartid.
struct rv_reservation {
    uint64_t addr;
    bool     valid;
};

// A whole emulated machine.
struct rv_machine {
    // RAM bounds; anything outside is either a hole or MMIO.
    uint64_t               ram_start, ram_end;
    // Virtual machine's RAM.
    uint8_t               *ram;
    // Table of trap interception functions.
    rv_trap_fn_t           trap_hook[32];
    // Cookie sent to all hook functions.
    void                  *hook_cookie;
    // CPU array; owned by this machine when allocated via rv_machine_init.
    struct rv_cpu         *cpus;
    size_t                 cpu_count;
    // Serialises all LR/SC/AMO operations across CPUs (Stage 4+).
    pthread_mutex_t        atomic_lock;
    // One LR reservation per hart, indexed by mhartid; protected by
    // atomic_lock.
    struct rv_reservation *reservations;
};

// Initialise a machine: allocate cpu_count CPUs, RAM of ram_size bytes at
// ram_start, and set up the atomic_lock and reservations array. Sets mhartid =
// i for each CPU.
bool rv_machine_init(
    struct rv_machine *machine,
    size_t             cpu_count,
    uint64_t           ram_start,
    size_t             ram_size
);
// Run all CPUs concurrently (one pthread each) until every cpu->halted is set.
void rv_machine_run(struct rv_machine *machine);
// Free all resources allocated by rv_machine_init.
void rv_machine_destroy(struct rv_machine *machine);

// Partial access to physical memory (e.g. spanning virtual page boundary).
bool rv_access_phys_partial(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    void              *data,
    size_t             size,
    enum rv_access     mode
);
// Access physical memory, optimizing for aligned access.
bool rv_access_phys(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           addr,
    void              *data,
    uint8_t            size_exp,
    enum rv_access     mode
);

// Macro that tries to read from RAM.
#define RV_READ_RAM(machine, data_type, addr_var, dest_var, fail_code)         \
    if ((addr_var) >= (machine).ram_start &&                                   \
        (addr_var) <= (machine).ram_end - sizeof(data_type)) {                 \
        dest_var =                                                             \
            *(data_type *)((machine).ram + (addr_var) - (machine).ram_start);  \
    } else {                                                                   \
        fail_code                                                              \
    }

// Macro that tries to write to RAM.
#define RV_WRITE_RAM(machine, data_type, addr_var, source_var, fail_code)      \
    if ((addr_var) >= (machine).ram_start &&                                   \
        (addr_var) <= (machine).ram_end - sizeof(data_type)) {                 \
        *(data_type *)((machine).ram + (addr_var) - (machine).ram_start) =     \
            source_var;                                                        \
    } else {                                                                   \
        fail_code                                                              \
    }
