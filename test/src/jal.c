
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "testcase.h"

TESTCASE(jal_encoding, {
    rv_forcefeed_insn(machine, cpu, 0x2401e06f);
    TEST_PC(123456);

    cpu->pc = 0;
    rv_forcefeed_insn(machine, cpu, 0x42ba206f);
    TEST_PC(666666);

    cpu->pc = 0;
    rv_forcefeed_insn(machine, cpu, 0x23ef406f);
    TEST_PC(999998);

    cpu->pc = 0;
    rv_forcefeed_insn(machine, cpu, 0x3ee8406f);
    TEST_PC(541678);
})
