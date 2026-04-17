
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

// A whole emulated machine.
struct rv_machine {
    // RAM bounds; anything outside is either a hole or MMIO.
    uint64_t ram_start, ram_end;
    // Virtual machine's RAM.
    uint8_t *ram;
};

// Macro that tries to read from RAM.
#define RV_READ_RAM(machine, data_type, addr_var, dest_var, fail_code)                                                 \
    if ((addr_var) >= (machine).ram_start && (addr_var) <= (machine).ram_end - sizeof(data_type)) {                    \
        dest_var = *(data_type *)((machine).ram + (addr_var));                                                         \
    } else {                                                                                                           \
        fail_code                                                                                                      \
    }


// Macro that tries to write to RAM.
#define RV_WRITE_RAM(machine, data_type, addr_var, source_var, fail_code)                                              \
    if ((addr_var) >= (machine).ram_start && (addr_var) <= (machine).ram_end - sizeof(data_type)) {                    \
        *(data_type *)((machine).ram + (addr_var)) = source_var;                                                       \
    } else {                                                                                                           \
        fail_code                                                                                                      \
    }
