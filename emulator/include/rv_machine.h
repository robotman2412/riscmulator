
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

struct rv_machine;
struct rv_cpu;

// Callback to run if the CPU performs an ECALL instruction (instead of the normal mechanism).
// Note that it is up to the callee to increment the PC by 4 to continue execution, if desired.
typedef void (*rv_ecall_fn_t)(struct rv_machine *machine, struct rv_cpu *cpu);

// A whole emulated machine.
struct rv_machine {
    // RAM bounds; anything outside is either a hole or MMIO.
    uint64_t      ram_start, ram_end;
    // Virtual machine's RAM.
    uint8_t      *ram;
    // Intercept ECALL from U-mode if set.
    rv_ecall_fn_t ecall_u;
    // Intercept ECALL from S-mode if set.
    rv_ecall_fn_t ecall_s;
    // Intercept ECALL from M-mode if set.
    rv_ecall_fn_t ecall_m;
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
