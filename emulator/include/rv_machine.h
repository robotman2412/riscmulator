
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_privileged.h"

#include <stdint.h>

struct rv_machine;
struct rv_cpu;

// Callback to run instead of the normal trap mechanism.
// If this returns true, the normal trap mechanism activates.
typedef bool (*rv_trap_fn_t)(void *cookie, struct rv_machine *machine, struct rv_cpu *cpu, struct rv_trap trap);

// A whole emulated machine.
struct rv_machine {
    // RAM bounds; anything outside is either a hole or MMIO.
    uint64_t     ram_start, ram_end;
    // Virtual machine's RAM.
    uint8_t     *ram;
    // Table of trap interception functions.
    rv_trap_fn_t trap_hook[32];
    // Cookie sent to all hook functions.
    void        *hook_cookie;
};

// Macro that tries to read from RAM.
#define RV_READ_RAM(machine, data_type, addr_var, dest_var, fail_code)                                                 \
    if ((addr_var) >= (machine).ram_start && (addr_var) <= (machine).ram_end - sizeof(data_type)) {                    \
        dest_var = *(data_type *)((machine).ram + (addr_var) - (machine).ram_start);                                   \
    } else {                                                                                                           \
        fail_code                                                                                                      \
    }

// Macro that tries to write to RAM.
#define RV_WRITE_RAM(machine, data_type, addr_var, source_var, fail_code)                                              \
    if ((addr_var) >= (machine).ram_start && (addr_var) <= (machine).ram_end - sizeof(data_type)) {                    \
        *(data_type *)((machine).ram + (addr_var) - (machine).ram_start) = source_var;                                 \
    } else {                                                                                                           \
        fail_code                                                                                                      \
    }
