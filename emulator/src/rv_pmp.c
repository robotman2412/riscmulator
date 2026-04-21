
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_pmp.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"

#include <stddef.h>
#include <stdint.h>

// Check whether paddr falls inside the NAPOT region encoded by pmpaddr_val.
// pmpaddr encodes: ...AAA0111...1 where N trailing 1s → size = 2^(N+3) bytes.
[[gnu::always_inline]]
static inline bool pmpaddr_match(uint64_t paddr, uint64_t pmpaddr_val) {
    // qmask has N+1 bits set (covers trailing 1s plus the 0 boundary bit).
    uint64_t qmask = pmpaddr_val ^ (pmpaddr_val + 1);
    // bmask is the byte-level region mask: (qmask << 2) | 3 = size_bytes - 1.
    uint64_t bmask = (qmask << 2) | 3;
    uint64_t base  = (pmpaddr_val & ~qmask) << 2;
    return (paddr & ~bmask) == base;
}

// Check PMP access permissions for a certain address and size.
// Returns the access permission bits seen in the same format as `pmpcfg`.
uint8_t rv_pmp_check(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    uint64_t           paddr,
    uint8_t            size_exp,
    bool               m_mode
) {
    (void)machine;
    uint64_t paddr1 = paddr + (1 << size_exp) - 1;

    for (size_t i = 0; i < 64; i++) {
        uint8_t cfg     = cpu->csr.pmpcfg.unpacked[i];
        bool    locked  = cfg & (1 << RV_PMPCFG_L_BIT);
        bool    enabled = cfg & (2 << RV_PMPCFG_A_BASE_BIT);
        if (!enabled || (m_mode && !locked)) {
            continue;
        }
        bool match0 = pmpaddr_match(paddr, cpu->csr.pmpaddr[i]);
        bool match1 = pmpaddr_match(paddr1, cpu->csr.pmpaddr[i]);
        if (match0 != match1) {
            // Spans edge of this PMP range; access is always forbidden.
            return 0;
        } else if (!match0) {
            // Not contained in this range.
            continue;
        }
        // Contained in this range.
        return cfg & 7;
    }

    return m_mode ? 7 : 0;
}
