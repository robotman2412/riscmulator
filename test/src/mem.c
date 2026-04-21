
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_machine.h"
#include "rv_privileged.h"
#include "testcase.h"

TESTCASE(store_insn, {
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

// Hook that records which trap causes fired, suppressing the normal trap path.
static bool trap_catch_hook(
    void              *cookie,
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    struct rv_trap     trap
) {
    (void)machine;
    (void)cpu;
    bool *caught       = cookie;
    caught[trap.cause] = true;
    return true;
}

// Aligned reads

TESTCASE(phys_read_aligned, {
    machine->ram[0x080] = 0xAB;

    machine->ram[0x100] = 0x34;
    machine->ram[0x101] = 0x12;

    machine->ram[0x200] = 0x78;
    machine->ram[0x201] = 0x56;
    machine->ram[0x202] = 0x34;
    machine->ram[0x203] = 0x12;

    machine->ram[0x300] = 0x08;
    machine->ram[0x301] = 0x07;
    machine->ram[0x302] = 0x06;
    machine->ram[0x303] = 0x05;
    machine->ram[0x304] = 0x04;
    machine->ram[0x305] = 0x03;
    machine->ram[0x306] = 0x02;
    machine->ram[0x307] = 0x01;

    uint64_t data;

    data = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10080, &data, 0, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0xAB);

    data = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10100, &data, 1, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0x1234);

    data = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10200, &data, 2, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0x12345678);

    data = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10300, &data, 3, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0x0102030405060708);
})

// Aligned writes

TESTCASE(phys_write_aligned, {
    uint64_t data;

    data = 0xAB;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10080, &data, 0, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x080] == 0xAB);

    data = 0x1234;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10100, &data, 1, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x100] == 0x34);
    TEST_ASSERT(machine->ram[0x101] == 0x12);

    data = 0x12345678;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10200, &data, 2, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x200] == 0x78);
    TEST_ASSERT(machine->ram[0x201] == 0x56);
    TEST_ASSERT(machine->ram[0x202] == 0x34);
    TEST_ASSERT(machine->ram[0x203] == 0x12);

    data = 0x0102030405060708;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10300, &data, 3, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x300] == 0x08);
    TEST_ASSERT(machine->ram[0x301] == 0x07);
    TEST_ASSERT(machine->ram[0x302] == 0x06);
    TEST_ASSERT(machine->ram[0x303] == 0x05);
    TEST_ASSERT(machine->ram[0x304] == 0x04);
    TEST_ASSERT(machine->ram[0x305] == 0x03);
    TEST_ASSERT(machine->ram[0x306] == 0x02);
    TEST_ASSERT(machine->ram[0x307] == 0x01);
})

// Misaligned reads (fast path: both endpoints within RAM)

TESTCASE(phys_read_misaligned, {
    // Half at odd address (addr % 2 == 1).
    machine->ram[0x001] = 0xCD;
    machine->ram[0x002] = 0xEF;
    uint64_t data       = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10001, &data, 1, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0xEFCD);

    // Word at 2-byte-aligned address (addr % 4 == 2).
    machine->ram[0x012] = 0x78;
    machine->ram[0x013] = 0x56;
    machine->ram[0x014] = 0x34;
    machine->ram[0x015] = 0x12;
    data                = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10012, &data, 2, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0x12345678);

    // Dword at 4-byte-aligned address (addr % 8 == 4).
    machine->ram[0x024] = 0x08;
    machine->ram[0x025] = 0x07;
    machine->ram[0x026] = 0x06;
    machine->ram[0x027] = 0x05;
    machine->ram[0x028] = 0x04;
    machine->ram[0x029] = 0x03;
    machine->ram[0x02A] = 0x02;
    machine->ram[0x02B] = 0x01;
    data                = 0;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10024, &data, 3, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(data == 0x0102030405060708);
})

// Misaligned writes (fast path: both endpoints within RAM)

TESTCASE(phys_write_misaligned, {
    uint64_t data;

    // Half at odd address (addr % 2 == 1).
    data = 0x1234;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10001, &data, 1, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x001] == 0x34);
    TEST_ASSERT(machine->ram[0x002] == 0x12);

    // Word at 2-byte-aligned address (addr % 4 == 2).
    data = 0x12345678;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10012, &data, 2, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x012] == 0x78);
    TEST_ASSERT(machine->ram[0x013] == 0x56);
    TEST_ASSERT(machine->ram[0x014] == 0x34);
    TEST_ASSERT(machine->ram[0x015] == 0x12);

    // Dword at 4-byte-aligned address (addr % 8 == 4).
    data = 0x0102030405060708;
    TEST_ASSERT(
        rv_access_phys(machine, cpu, 0x10024, &data, 3, RV_ACCESS_STORE)
    );
    TEST_ASSERT(machine->ram[0x024] == 0x08);
    TEST_ASSERT(machine->ram[0x025] == 0x07);
    TEST_ASSERT(machine->ram[0x026] == 0x06);
    TEST_ASSERT(machine->ram[0x027] == 0x05);
    TEST_ASSERT(machine->ram[0x028] == 0x04);
    TEST_ASSERT(machine->ram[0x029] == 0x03);
    TEST_ASSERT(machine->ram[0x02A] == 0x02);
    TEST_ASSERT(machine->ram[0x02B] == 0x01);
})

// Faults: access entirely outside RAM

TESTCASE(phys_fault_load_outside, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    uint64_t data = 0;
    TEST_ASSERT(
        !rv_access_phys(machine, cpu, 0x08000, &data, 3, RV_ACCESS_LOAD)
    );
    TEST_ASSERT(caught[RV_CAUSE_LACCESS]);
    TEST_ASSERT(cpu->csr.mtval == 0x08000);
})

TESTCASE(phys_fault_store_outside, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    uint64_t data = 0xDEAD;
    TEST_ASSERT(
        !rv_access_phys(machine, cpu, 0x20000, &data, 3, RV_ACCESS_STORE)
    );
    TEST_ASSERT(caught[RV_CAUSE_SACCESS]);
    TEST_ASSERT(cpu->csr.mtval == 0x20000);
})

// Faults: aligned access spanning the end of RAM
//
// Trim 4 bytes from ram_end to create a non-8-byte-aligned boundary, then do
// an aligned dword access whose last 4 bytes fall outside the trimmed region.
// Because the access is aligned, rv_access_phys takes the invalid-access path
// (not misaligned_access) and fires the appropriate trap.

TESTCASE(phys_fault_load_edge, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    uint64_t data = 0;
    TEST_ASSERT(!rv_access_phys(
        machine,
        cpu,
        machine->ram_start - 4,
        &data,
        3,
        RV_ACCESS_LOAD
    ));
    TEST_ASSERT(caught[RV_CAUSE_LACCESS]);
    TEST_ASSERT(cpu->csr.mtval == machine->ram_start - 4);
})

TESTCASE(phys_fault_store_edge, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    uint64_t data = 0;
    TEST_ASSERT(!rv_access_phys(
        machine,
        cpu,
        machine->ram_end - 4,
        &data,
        3,
        RV_ACCESS_STORE
    ));
    TEST_ASSERT(caught[RV_CAUSE_SACCESS]);
    TEST_ASSERT(cpu->csr.mtval == machine->ram_end);
})
