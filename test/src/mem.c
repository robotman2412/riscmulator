
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_machine.h"
#include "testcase.h"

TESTCASE(store, {
    cpu->xregs[2] = 0x10800;

    cpu->xregs[1] = 0xcc;
    rv_forcefeed_insn(machine, cpu, 0x161104a3); // sb x1, 361(x2)
    TEST_ASSERT(machine->ram[0x800 + 361] == 0xcc);

    cpu->xregs[1] = 0xf00d;
    rv_forcefeed_insn(machine, cpu, 0xfc111ca3); // sh x1, -39(x2)
    TEST_ASSERT(machine->ram[0x800 - 39] == 0x0d);
    TEST_ASSERT(machine->ram[0x800 - 38] == 0xf0);

    cpu->xregs[1] = 0xcafebabe;
    rv_forcefeed_insn(machine, cpu, 0x001122a3); // sw x1, 5(x2)
    TEST_ASSERT(machine->ram[0x800 + 5] == 0xbe);
    TEST_ASSERT(machine->ram[0x800 + 6] == 0xba);
    TEST_ASSERT(machine->ram[0x800 + 7] == 0xfe);
    TEST_ASSERT(machine->ram[0x800 + 8] == 0xca);

    cpu->xregs[1] = 0xbbb69420eff13337;
    rv_forcefeed_insn(machine, cpu, 0x441135a3); // sd x1, 1099(x2)
    TEST_ASSERT(machine->ram[0x800 + 1099] == 0x37);
    TEST_ASSERT(machine->ram[0x800 + 1100] == 0x33);
    TEST_ASSERT(machine->ram[0x800 + 1101] == 0xf1);
    TEST_ASSERT(machine->ram[0x800 + 1102] == 0xef);
    TEST_ASSERT(machine->ram[0x800 + 1103] == 0x20);
    TEST_ASSERT(machine->ram[0x800 + 1104] == 0x94);
    TEST_ASSERT(machine->ram[0x800 + 1105] == 0xb6);
    TEST_ASSERT(machine->ram[0x800 + 1106] == 0xbb);
})
