
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "cpu/rv_cpu.h"
#include "emu_machine.h"
#include "cpu/rv_privileged.h"
#include "testcase.h"

#include <string.h>

// Trap hook that halts the CPU on ECALL and records unexpected traps.
static bool halt_on_ecall(void *cookie, struct emu_machine *machine, struct rv_cpu *cpu, struct rv_trap trap) {
    (void)machine;
    bool *failed = cookie;
    cpu->halted  = true;
    if (trap.cause != RV_CAUSE_ECALL_M) {
        *failed = true;
    }
    return false;
}

static void setup_hooks(struct emu_machine *machine, bool *failed) {
    machine->hook_cookie = failed;
    for (int i = 0; i < 32; i++) {
        machine->trap_hook[i] = &halt_on_ecall;
    }
}

// Single-CPU smoke test: addi x1,x0,5; addi x2,x0,3; add x3,x1,x2; ecall → x3=8
TESTCASE_NOMACHINE(multicpu_single, {
    static uint32_t const code[] = {
        0x00500093, // addi x1, x0, 5
        0x00300113, // addi x2, x0, 3
        0x002081B3, // add  x3, x1, x2
        0x00000073, // ecall
    };

    struct emu_machine machine = {0};
    bool              failed  = false;
    TEST_ASSERT(emu_machine_init(&machine, 1, 0x10000, sizeof(code)));
    memcpy(machine.ram, code, sizeof(code));
    machine.cpus[0].privilege = 3;
    machine.cpus[0].pc        = 0x10000;
    setup_hooks(&machine, &failed);

    emu_machine_run(&machine);

    TEST_ASSERT(!failed);
    TEST_ASSERT((int64_t)machine.cpus[0].xregs[3] == 8);
    emu_machine_destroy(&machine);
})

// Two-CPU independence test: CPU0 computes 10+20=30, CPU1 computes 100+200=300, both in x3.
TESTCASE_NOMACHINE(multicpu_two_cpus, {
    static uint32_t const code0[] = {
        0x00A00093, // addi x1, x0, 10
        0x01400113, // addi x2, x0, 20
        0x002081B3, // add  x3, x1, x2
        0x00000073, // ecall
    };
    static uint32_t const code1[] = {
        0x06400093, // addi x1, x0, 100
        0x0C800113, // addi x2, x0, 200
        0x002081B3, // add  x3, x1, x2
        0x00000073, // ecall
    };

    size_t const      ram_size = 0x200;
    struct emu_machine machine  = {0};
    bool              failed   = false;
    TEST_ASSERT(emu_machine_init(&machine, 2, 0x10000, ram_size));
    memcpy(machine.ram, code0, sizeof(code0));
    memcpy(machine.ram + 0x100, code1, sizeof(code1));
    machine.cpus[0].privilege = 3;
    machine.cpus[0].pc        = 0x10000;
    machine.cpus[1].privilege = 3;
    machine.cpus[1].pc        = 0x10100;
    setup_hooks(&machine, &failed);

    emu_machine_run(&machine);

    TEST_ASSERT(!failed);
    TEST_ASSERT((int64_t)machine.cpus[0].xregs[3] == 30);
    TEST_ASSERT((int64_t)machine.cpus[1].xregs[3] == 300);
    emu_machine_destroy(&machine);
})

// Hart-ID test: both CPUs run csrrs x1,mhartid,x0; ecall.
// CPU0 → x1=0, CPU1 → x1=1.
TESTCASE_NOMACHINE(multicpu_hartid, {
    static uint32_t const code[] = {
        0xF14020F3, // csrrs x1, mhartid, x0
        0x00000073, // ecall
    };

    struct emu_machine machine = {0};
    bool              failed  = false;
    TEST_ASSERT(emu_machine_init(&machine, 2, 0x10000, sizeof(code)));
    memcpy(machine.ram, code, sizeof(code));
    machine.cpus[0].privilege = 3;
    machine.cpus[0].pc        = 0x10000;
    machine.cpus[1].privilege = 3;
    machine.cpus[1].pc        = 0x10000;
    setup_hooks(&machine, &failed);

    emu_machine_run(&machine);

    TEST_ASSERT(!failed);
    TEST_ASSERT((int64_t)machine.cpus[0].xregs[1] == 0);
    TEST_ASSERT((int64_t)machine.cpus[1].xregs[1] == 1);
    emu_machine_destroy(&machine);
})
