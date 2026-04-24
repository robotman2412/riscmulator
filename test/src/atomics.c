
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_machine.h"
#include "rv_privileged.h"
#include "testcase.h"

// Captures the most recent trap for inspection.
struct trap_capture {
    uint64_t cause, tval;
    bool     fired;
};

static bool capture_trap(void *cookie, struct rv_machine *m, struct rv_cpu *cpu, struct rv_trap trap) {
    (void)m;
    (void)cpu;
    struct trap_capture *c = cookie;
    c->cause               = trap.cause;
    c->tval                = trap.tval;
    c->fired               = true;
    return false;
}

static void setup_trap_hook(struct rv_machine *machine, struct trap_capture *cap) {
    cap->fired           = false;
    machine->hook_cookie = cap;
    for (int i = 0; i < 32; i++) {
        machine->trap_hook[i] = capture_trap;
    }
}

// Instruction encodings: all use rs1=x1 (address), rs2=x2 (data), rd=x3 (result).
// LR.W  x3, (x1)      funct5=00010, funct3=010, rs2=x0
#define LRW      0x1000A1AFu
// LR.D  x3, (x1)      funct5=00010, funct3=011, rs2=x0
#define LRD      0x1000B1AFu
// SC.W  x3, x2, (x1)  funct5=00011, funct3=010
#define SCW      0x1820A1AFu
// SC.D  x3, x2, (x1)  funct5=00011, funct3=011
#define SCD      0x1820B1AFu
// AMOADD.W x3, x2, (x1) funct5=00000, funct3=010
#define AMOADD_W 0x0020A1AFu
// AMOADD.D x3, x2, (x1) funct5=00000, funct3=011
#define AMOADD_D 0x0020B1AFu

// LR.W requires 4-byte alignment; misalign traps RV_CAUSE_LALIGN.
TESTCASE(amo_misalign_lrw, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap.
    cpu->xregs[1] = 0x10000;
    rv_forcefeed_insn(machine, cpu, LRW);
    TEST_ASSERT(!cap.fired);

    // 1-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10001;
    rv_forcefeed_insn(machine, cpu, LRW);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_LALIGN);
    TEST_ASSERT(cap.tval == 0x10001);

    // 2-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10002;
    rv_forcefeed_insn(machine, cpu, LRW);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_LALIGN);
    TEST_ASSERT(cap.tval == 0x10002);

    // 3-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10003;
    rv_forcefeed_insn(machine, cpu, LRW);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_LALIGN);
    TEST_ASSERT(cap.tval == 0x10003);
})

// LR.D requires 8-byte alignment; misalign traps RV_CAUSE_LALIGN.
TESTCASE(amo_misalign_lrd, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap.
    cpu->xregs[1] = 0x10000;
    rv_forcefeed_insn(machine, cpu, LRD);
    TEST_ASSERT(!cap.fired);

    // 4-byte aligned but not 8-byte.
    cap.fired     = false;
    cpu->xregs[1] = 0x10004;
    rv_forcefeed_insn(machine, cpu, LRD);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_LALIGN);
    TEST_ASSERT(cap.tval == 0x10004);

    // 1-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10001;
    rv_forcefeed_insn(machine, cpu, LRD);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_LALIGN);
    TEST_ASSERT(cap.tval == 0x10001);
})

// SC.W requires 4-byte alignment; misalign traps RV_CAUSE_SALIGN.
TESTCASE(amo_misalign_scw, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap (SC fails silently, rd=1, no prior LR).
    cpu->xregs[1] = 0x10000;
    cpu->xregs[2] = 0xDEAD;
    rv_forcefeed_insn(machine, cpu, SCW);
    TEST_ASSERT(!cap.fired);

    // 1-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10001;
    rv_forcefeed_insn(machine, cpu, SCW);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10001);

    // 3-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10003;
    rv_forcefeed_insn(machine, cpu, SCW);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10003);
})

// SC.D requires 8-byte alignment; misalign traps RV_CAUSE_SALIGN.
TESTCASE(amo_misalign_scd, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap.
    cpu->xregs[1] = 0x10000;
    cpu->xregs[2] = 0xDEAD;
    rv_forcefeed_insn(machine, cpu, SCD);
    TEST_ASSERT(!cap.fired);

    // 4-byte aligned but not 8-byte.
    cap.fired     = false;
    cpu->xregs[1] = 0x10004;
    rv_forcefeed_insn(machine, cpu, SCD);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10004);
})

// AMOADD.W requires 4-byte alignment; misalign traps RV_CAUSE_SALIGN.
TESTCASE(amo_misalign_amoadd_w, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap.
    cpu->xregs[1] = 0x10000;
    cpu->xregs[2] = 1;
    rv_forcefeed_insn(machine, cpu, AMOADD_W);
    TEST_ASSERT(!cap.fired);

    // 1-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10001;
    rv_forcefeed_insn(machine, cpu, AMOADD_W);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10001);

    // 2-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10002;
    rv_forcefeed_insn(machine, cpu, AMOADD_W);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10002);
})

// AMOADD.D requires 8-byte alignment; misalign traps RV_CAUSE_SALIGN.
TESTCASE(amo_misalign_amoadd_d, {
    struct trap_capture cap;
    setup_trap_hook(machine, &cap);
    cpu->privilege = 3;

    // Aligned — must NOT trap.
    cpu->xregs[1] = 0x10000;
    cpu->xregs[2] = 1;
    rv_forcefeed_insn(machine, cpu, AMOADD_D);
    TEST_ASSERT(!cap.fired);

    // 4-byte aligned but not 8-byte.
    cap.fired     = false;
    cpu->xregs[1] = 0x10004;
    rv_forcefeed_insn(machine, cpu, AMOADD_D);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10004);

    // 1-byte off.
    cap.fired     = false;
    cpu->xregs[1] = 0x10001;
    rv_forcefeed_insn(machine, cpu, AMOADD_D);
    TEST_ASSERT(cap.fired);
    TEST_ASSERT(cap.cause == RV_CAUSE_SALIGN);
    TEST_ASSERT(cap.tval == 0x10001);
})
