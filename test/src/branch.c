
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "testcase.h"

TESTCASE(jal_encoding, {
    cpu->epc = 0;

    rv_forcefeed_insn(machine, cpu, 0x2401e06f);
    TEST_PC(cpu->epc + 123456);

    rv_forcefeed_insn(machine, cpu, 0x42ba206f);
    TEST_PC(cpu->epc + 666666);

    rv_forcefeed_insn(machine, cpu, 0x23ef406f);
    TEST_PC(cpu->epc + 999998);

    rv_forcefeed_insn(machine, cpu, 0x3ee8406f);
    TEST_PC(cpu->epc + 541678);
})

TESTCASE(branch_encoding, {
    cpu->epc = 0;

    rv_forcefeed_insn(machine, cpu, 0x3e000f63);
    TEST_PC(cpu->epc + 1022);

    rv_forcefeed_insn(machine, cpu, 0xc00000e3);
    TEST_PC(cpu->epc - 1024);

    rv_forcefeed_insn(machine, cpu, 0x34000063);
    TEST_PC(cpu->epc + 832);

    rv_forcefeed_insn(machine, cpu, 0xc60007e3);
    TEST_PC(cpu->epc - 914);
})
