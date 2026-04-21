
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_csr.h"
#include "rv_cpu.h"
#include "rv_machine.h"
#include "testcase.h"

// Helpers to read/write CSRs directly (machine mode).
static bool csr_read(struct rv_cpu *cpu, uint32_t index, uint64_t *out) {
    cpu->privilege = 3;
    return rv_csr_read(cpu, index, out);
}

static bool csr_write(struct rv_cpu *cpu, uint32_t index, uint64_t val) {
    cpu->privilege = 3;
    return rv_csr_write(cpu, index, val);
}

// Basic pmpcfg read/write (pmpcfg0 = 0x3A0, packed word 0).
TESTCASE(pmp_pmpcfg_basic, {
    (void)machine;
    uint64_t val;

    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0F0F0F0F0F0F0F0FULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x0F0F0F0F0F0F0F0FULL)

    // pmpcfg2 (0x3A2) maps to packed word 1.
    // Use a value with no reserved bits [6:5] set in any byte (all survive & 0x9F).
    TEST_ASSERT(csr_write(cpu, 0x3A2, 0x0102030405060708ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A2, &val))
    TEST_ASSERT(val == 0x0102030405060708ULL)
    // pmpcfg0 should be unaffected.
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x0F0F0F0F0F0F0F0FULL)
})

// Reserved bits [6:5] in each pmpcfg byte must be hardwired to 0.
TESTCASE(pmp_pmpcfg_reserved_bits, {
    (void)machine;
    uint64_t val;
    // Write all-ones; bits [6:5] of each byte must be cleared on readback.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0xFFFFFFFFFFFFFFFFULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    // 0x9F per byte.
    TEST_ASSERT(val == 0x9F9F9F9F9F9F9F9FULL)
})

// Odd pmpcfg indices (0x3A1, 0x3A3, ...) are illegal in RV64.
TESTCASE(pmp_pmpcfg_odd_illegal, {
    (void)machine;
    uint64_t val;
    TEST_ASSERT(!csr_read(cpu, 0x3A1, &val))
    TEST_ASSERT(!csr_write(cpu, 0x3A1, 0))
    TEST_ASSERT(!csr_read(cpu, 0x3A3, &val))
    TEST_ASSERT(!csr_write(cpu, 0x3A9, 0))
})

// Basic pmpaddr read/write.
TESTCASE(pmp_pmpaddr_basic, {
    (void)machine;
    uint64_t val;

    TEST_ASSERT(csr_write(cpu, 0x3B0, 0xDEADBEEFCAFEBABEULL)) // pmpaddr0
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == 0xDEADBEEFCAFEBABEULL)

    TEST_ASSERT(csr_write(cpu, 0x3B5, 0x1234567890ABCDEFULL)) // pmpaddr5
    TEST_ASSERT(csr_read(cpu, 0x3B5, &val))
    TEST_ASSERT(val == 0x1234567890ABCDEFULL)

    // pmpaddr0 should be unaffected.
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == 0xDEADBEEFCAFEBABEULL)

    // Last pmpaddr63 (0x3EF).
    TEST_ASSERT(csr_write(cpu, 0x3EF, 0xAAAAAAAAAAAAAAAAULL))
    TEST_ASSERT(csr_read(cpu, 0x3EF, &val))
    TEST_ASSERT(val == 0xAAAAAAAAAAAAAAAAULL)
})

// Locked pmpcfg byte: the locked byte must not change on write.
TESTCASE(pmp_pmpcfg_lock, {
    (void)machine;
    uint64_t val;

    // Write pmpcfg0 with byte0=0x8F (L=1, RWX, OFF mode) and byte1=0x07 (unlocked).
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x000000000000078FULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x000000000000078FULL)

    // Attempt to overwrite both bytes; only byte1 should change.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0000000000001100ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    // byte0 stays 0x8F (locked), byte1 becomes 0x11 & 0x9F = 0x11.
    TEST_ASSERT(val == 0x000000000000118FULL)
})

// Locked pmpaddr: write to pmpaddr[i] is ignored when pmpcfg[i].L=1.
TESTCASE(pmp_pmpaddr_lock_direct, {
    (void)machine;
    uint64_t val;

    // Set pmpaddr1 to a known value.
    TEST_ASSERT(csr_write(cpu, 0x3B1, 0x1111111111111111ULL))
    // Lock pmpcfg entry 1 (byte 1 of pmpcfg0): L=1.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0000000000008000ULL))

    // Attempt overwrite of pmpaddr1; must be silently ignored.
    TEST_ASSERT(csr_write(cpu, 0x3B1, 0xFFFFFFFFFFFFFFFFULL))
    TEST_ASSERT(csr_read(cpu, 0x3B1, &val))
    TEST_ASSERT(val == 0x1111111111111111ULL)
})

// TOR-locked next entry also locks the preceding pmpaddr.
TESTCASE(pmp_pmpaddr_lock_tor, {
    (void)machine;
    uint64_t val;

    // Set pmpaddr2 to known value.
    TEST_ASSERT(csr_write(cpu, 0x3B2, 0x2222222222222222ULL))

    // Lock pmpcfg entry 3 (byte 3 of pmpcfg0) with L=1 and A=TOR (0b01 << 3).
    // Byte 3 = L|A=TOR = 0x80 | 0x08 = 0x88; mask to 0x9F → 0x88.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0000000088000000ULL))

    // pmpaddr2 (addr_idx=2, next entry 3 is TOR-locked) must be locked.
    TEST_ASSERT(csr_write(cpu, 0x3B2, 0xFFFFFFFFFFFFFFFFULL))
    TEST_ASSERT(csr_read(cpu, 0x3B2, &val))
    TEST_ASSERT(val == 0x2222222222222222ULL)
})

// Privilege check: S-mode (privilege=1) cannot access M-mode PMP CSRs (bits[9:8]=3).
TESTCASE(pmp_privilege_check, {
    (void)machine;
    uint64_t val;
    cpu->privilege = 1;
    TEST_ASSERT(!rv_csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(!rv_csr_write(cpu, 0x3A0, 0))
    TEST_ASSERT(!rv_csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(!rv_csr_write(cpu, 0x3B0, 0))
})
