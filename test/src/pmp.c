
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"
#include "rv_pmp.h"
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

    // 0x1F = L=0, A=NAPOT, RWX=all.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x1F1F1F1F1F1F1F1FULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x1F1F1F1F1F1F1F1FULL)

    // pmpcfg2 (0x3A2) maps to packed word 1. 0x07 = L=0, A=OFF, RWX=all.
    TEST_ASSERT(csr_write(cpu, 0x3A2, 0x0707070707070707ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A2, &val))
    TEST_ASSERT(val == 0x0707070707070707ULL)
    // pmpcfg0 should be unaffected.
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x1F1F1F1F1F1F1F1FULL)
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

    // Default A=OFF: bits [G-1:0] of readback are forced to 0.
    TEST_ASSERT(csr_write(cpu, 0x3B0, 0xDEADBEEFCAFEBABEULL)) // pmpaddr0
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == (0xDEADBEEFCAFEBABEULL & ~RV_PMPGRAIN_OFF_MASK))

    TEST_ASSERT(csr_write(cpu, 0x3B5, 0x1234567890ABCDEFULL)) // pmpaddr5
    TEST_ASSERT(csr_read(cpu, 0x3B5, &val))
    TEST_ASSERT(val == (0x1234567890ABCDEFULL & ~RV_PMPGRAIN_OFF_MASK))

    // pmpaddr0 should be unaffected.
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == (0xDEADBEEFCAFEBABEULL & ~RV_PMPGRAIN_OFF_MASK))

    // Last pmpaddr63 (0x3EF).
    TEST_ASSERT(csr_write(cpu, 0x3EF, 0xAAAAAAAAAAAAAAAAULL))
    TEST_ASSERT(csr_read(cpu, 0x3EF, &val))
    TEST_ASSERT(val == (0xAAAAAAAAAAAAAAAAULL & ~RV_PMPGRAIN_OFF_MASK))
})

// Locked pmpcfg byte: the locked byte must not change on write.
TESTCASE(pmp_pmpcfg_lock, {
    (void)machine;
    uint64_t val;

    // byte0=0x9F (L=1, A=NAPOT, RWX=all), byte1=0x07 (unlocked, A=OFF,
    // RWX=all).
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x000000000000079FULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT(val == 0x000000000000079FULL)

    // Attempt to overwrite both bytes; only byte1 should change.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0000000000001F00ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    // byte0 stays 0x9F (locked), byte1 becomes 0x1F (A=NAPOT, RWX=all).
    TEST_ASSERT(val == 0x0000000000001F9FULL)
})

// Locked pmpaddr: write to pmpaddr[i] is ignored when pmpcfg[i].L=1.
TESTCASE(pmp_pmpaddr_lock_direct, {
    (void)machine;
    uint64_t val;

    // Use a grain-aligned value so the readback is predictable under OFF mode.
    TEST_ASSERT(csr_write(cpu, 0x3B1, 0x1111111111111000ULL))
    // Lock pmpcfg entry 1 (byte 1 of pmpcfg0): L=1, A=OFF → 0x80.
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x0000000000008000ULL))

    // Attempt overwrite of pmpaddr1; must be silently ignored.
    TEST_ASSERT(csr_write(cpu, 0x3B1, 0xFFFFFFFFFFFFFFFFULL))
    TEST_ASSERT(csr_read(cpu, 0x3B1, &val))
    // In OFF mode the bottom bits read as 0; value was grain-aligned so no
    // change.
    TEST_ASSERT(val == 0x1111111111111000ULL)
})

// TOR (A=0b01) is normalized to OFF; NA4 (A=0b10) is normalized to NAPOT.
TESTCASE(pmp_cfg_a_normalization, {
    (void)machine;
    uint64_t val;

    // 0x09 = A=TOR, R=1 → stored/read as 0x01 (A=OFF, R=1).
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x09ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT((val & 0xFF) == 0x01)

    // 0x11 = A=NA4, R=1 → stored/read as 0x19 (A=NAPOT, R=1).
    TEST_ASSERT(csr_write(cpu, 0x3A0, 0x11ULL))
    TEST_ASSERT(csr_read(cpu, 0x3A0, &val))
    TEST_ASSERT((val & 0xFF) == 0x19)
})

// Grain masking: pmpaddr bits [G-1:0] are forced to 0 in OFF mode; bits [G-2:0]
// forced to 1 in NAPOT mode.
TESTCASE(pmp_pmpaddr_grain_mask, {
    (void)machine;
    uint64_t val;

    // Write a value with all low bits set; in OFF mode (default) they read as
    // 0.
    TEST_ASSERT(csr_write(cpu, 0x3B0, 0xDEADBEEFFFFFFFFFULL))
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == (0xDEADBEEFFFFFFFFFULL & ~RV_PMPGRAIN_OFF_MASK))

    // Switch entry 0 to NAPOT; low bits [G-2:0] now read as 1; G-1 bit reads
    // from stored value.
    TEST_ASSERT(csr_write(cpu, 0x3A0, (uint64_t)RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT))
    TEST_ASSERT(csr_read(cpu, 0x3B0, &val))
    TEST_ASSERT(val == (0xDEADBEEFFFFFFFFFULL | RV_PMPGRAIN_NAPOT_MASK))
})

// Helper: set a single locked NAPOT entry (entry 0) with given permissions and
// address. pmpaddr encodes a 4KB NAPOT region: (addr >> 2) | 0x1FF.
static void set_pmp0_napot4k(struct rv_cpu *cpu, uint64_t base, uint8_t rwx) {
    cpu->privilege = 3;
    // L=1, A=NAPOT, permissions in bits [2:0].
    uint8_t cfg    = (1 << RV_PMPCFG_L_BIT) | (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT) | (rwx & 7);
    cpu->csr.pmpcfg.unpacked[0] = cfg;
    cpu->csr.pmpaddr[0]         = (base >> 2) | 0x1FF; // 4KB = 9 trailing ones
}

// NAPOT match: access fully inside a 4KB region returns the entry's
// permissions.
TESTCASE(pmp_check_napot_match, {
    (void)machine;
    set_pmp0_napot4k(cpu, 0x80000000, 5); // R=1, W=0, X=1

    // Single-byte access at start and near end of region.
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000000, 1, false) == 5)
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000FFF, 1, false) == 5)
    // 4-byte access fully inside.
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000100, 4, false) == 5)
})

// NAPOT no-match: access outside the 4KB region → no entry → S-mode returns 0.
TESTCASE(pmp_check_napot_no_match, {
    (void)machine;
    set_pmp0_napot4k(cpu, 0x80000000, 7);

    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80001000, 1, false) == 0)
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x7FFFF000, 1, false) == 0)
})

// Access spanning a NAPOT region boundary is always forbidden (returns 0).
TESTCASE(pmp_check_napot_span, {
    (void)machine;
    set_pmp0_napot4k(cpu, 0x80000000, 7);

    // 4-byte access starting 2 bytes before the end: spans end of region.
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000FFE, 4, false) == 0)
    // 4-byte access starting at last byte: also spans end.
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000FFF, 4, false) == 0)
})

// M-mode: unlocked entry is bypassed; locked entry applies.
TESTCASE(pmp_check_m_mode, {
    (void)machine;

    // Unlocked NAPOT entry at 0x80000000 with no permissions.
    cpu->csr.pmpcfg.unpacked[0] = (RV_PMP_ADDR_MATCH_NAPOT << RV_PMPCFG_A_BASE_BIT); // L=0
    cpu->csr.pmpaddr[0]         = (0x80000000 >> 2) | 0x1FF;

    // M-mode bypasses unlocked entries → default allow (returns 7).
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000100, 1, true) == 7)

    // Lock the entry with no permissions.
    cpu->csr.pmpcfg.unpacked[0] |= (1 << RV_PMPCFG_L_BIT);

    // M-mode now obeys the locked entry → returns 0 (no permissions).
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000100, 1, true) == 0)
})

// S-mode with no matching entry → denied (returns 0).
TESTCASE(pmp_check_no_entry, {
    (void)machine;
    // No PMP entries configured (all OFF).
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000000, 1, false) == 0)
    // M-mode with no entries → allowed (returns 7).
    TEST_ASSERT(rv_pmp_check(machine, cpu, 0x80000000, 1, true) == 7)
})
