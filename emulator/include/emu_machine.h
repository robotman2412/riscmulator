
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "cpu/rv_privileged.h"
#include "emu_device.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct emu_machine;
struct rv_cpu;
struct rv_clint;

// RAM access mode.
enum rv_access {
    // Instruction fetch access.
    RV_ACCESS_INSN,
    // Load access, but on fail raise store access fault instead.
    RV_ACCESS_AMO,
    // Load access.
    RV_ACCESS_LOAD,
    // Store access.
    RV_ACCESS_STORE,
};

// Result of a memory access attempt.
enum rv_mem_result {
    RV_MEM_OK,           // Access succeeded.
    RV_MEM_ACCESS_FAULT, // Physical access fault (PMP, unmapped, MMIO error).
    RV_MEM_PAGE_FAULT,   // Virtual memory page fault.
};

// Map a memory result + access mode to the correct trap cause.
static inline enum rv_cause rv_mem_cause(enum rv_mem_result r, enum rv_access mode) {
    if (r == RV_MEM_PAGE_FAULT) {
        switch (mode) {
            case RV_ACCESS_INSN: return RV_CAUSE_IPAGE;
            case RV_ACCESS_LOAD: return RV_CAUSE_LPAGE;
            case RV_ACCESS_AMO:
            case RV_ACCESS_STORE: return RV_CAUSE_SPAGE;
        }
    }
    switch (mode) {
        case RV_ACCESS_INSN: return RV_CAUSE_IACCESS;
        case RV_ACCESS_LOAD: return RV_CAUSE_LACCESS;
        case RV_ACCESS_AMO:
        case RV_ACCESS_STORE: return RV_CAUSE_SACCESS;
    }
    __builtin_unreachable();
}

// Callback to run instead of the normal trap mechanism.
// If this returns true, the normal trap mechanism activates.
typedef bool (*rv_trap_fn_t)(void *cookie, struct emu_machine *machine, struct rv_cpu *cpu, struct rv_trap trap);

// Per-hart LR reservation entry; one per CPU indexed by mhartid.
struct rv_reservation {
    uint64_t addr;
    bool     valid;
};

// A whole emulated machine.
struct emu_machine {
    // RAM bounds; anything outside is either a hole or MMIO.
    uint64_t                ram_start, ram_end;
    // Virtual machine's RAM.
    uint8_t                *ram;
    // Table of trap interception functions.
    rv_trap_fn_t            trap_hook[32];
    // Cookie sent to all hook functions.
    void                   *hook_cookie;
    // CPU array; owned by this machine when allocated via emu_machine_init.
    struct rv_cpu          *cpus;
    size_t                  cpu_count;
    // Serialises all LR/SC/AMO operations across CPUs (Stage 4+).
    pthread_mutex_t         atomic_lock;
    // One LR reservation per hart, indexed by mhartid; protected by
    // atomic_lock.
    struct rv_reservation  *reservations;
    // MMIO regions; scanned on every non-RAM access.
    struct emu_mmio_region *mmio;
    size_t                  mmio_count;
    // Optional CLINT device; NULL if not present. Set before emu_machine_run.
    struct rv_clint        *clint;
};

// Initialise a machine: allocate cpu_count CPUs, RAM of ram_size bytes at
// ram_start, and set up the atomic_lock and reservations array. Sets mhartid =
// i for each CPU.
bool emu_machine_init(struct emu_machine *machine, size_t cpu_count, uint64_t ram_start, size_t ram_size);
// Run all CPUs concurrently (one pthread each) until every cpu->halted is set.
void emu_machine_run(struct emu_machine *machine);
// Free all resources allocated by emu_machine_init.
void emu_machine_destroy(struct emu_machine *machine);
// Append an MMIO region to the machine's dispatch table (copied by value).
// Returns false on allocation failure.
bool emu_machine_add_mmio(struct emu_machine *machine, struct emu_mmio_region region);

// Partial access to physical memory (e.g. spanning virtual page boundary).
enum rv_mem_result rv_access_phys_partial(
    struct emu_machine *machine,
    struct rv_cpu      *cpu,
    uint64_t            addr,
    void               *data,
    size_t              size,
    enum rv_access      mode,
    bool                ignore_pmp
);
// Access physical memory, optimizing for aligned access.
enum rv_mem_result rv_access_phys(
    struct emu_machine *machine,
    struct rv_cpu      *cpu,
    uint64_t            addr,
    void               *data,
    uint8_t             size_exp,
    enum rv_access      mode,
    bool                ignore_pmp
);
