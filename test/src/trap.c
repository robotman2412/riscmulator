
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "cpu/rv_cpu.h"
#include "emu_machine.h"
#include "cpu/rv_privileged.h"
#include "testcase.h"

static bool intercept_test_hook(void *cookie, struct emu_machine *machine, struct rv_cpu *cpu, struct rv_trap trap) {
    (void)machine;
    (void)cpu;
    bool *caught       = cookie;
    caught[trap.cause] = true;
    return false;
}

TESTCASE(ecall_hooks, {
    bool caught[32] = {0};

    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) {
        machine->trap_hook[i] = &intercept_test_hook;
    }

    cpu->privilege = 3;
    rv_forcefeed_insn(machine, cpu, 0x00000073);
    TEST_ASSERT(caught[RV_CAUSE_ECALL_M])

    cpu->privilege = 1;
    rv_forcefeed_insn(machine, cpu, 0x00000073);
    TEST_ASSERT(caught[RV_CAUSE_ECALL_S])

    cpu->privilege = 0;
    rv_forcefeed_insn(machine, cpu, 0x00000073);
    TEST_ASSERT(caught[RV_CAUSE_ECALL_U]);
})
