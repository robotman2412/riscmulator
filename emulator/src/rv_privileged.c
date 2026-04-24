
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_privileged.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"

#include <assert.h>
#include <stdatomic.h>

// Execute a certain trap handler.
// trap.cause < 32 for synchronous exceptions; bit 63 set for interrupts.
void rv_do_trap(
    struct rv_machine *machine, struct rv_cpu *cpu, struct rv_trap trap
) {
    bool     is_interrupt = (trap.cause >> 63) != 0;
    uint64_t cause_low    = trap.cause & 0x3f;

    if (!is_interrupt) {
        assert(trap.cause < 32);
        rv_trap_fn_t hook = machine->trap_hook[trap.cause];
        if (hook && !hook(machine->hook_cookie, machine, cpu, trap)) {
            return;
        }
    }

    // Delegate to S-mode if the CPU is currently below M-mode and the
    // relevant delegation bit is set.
    uint64_t deleg_mask = is_interrupt ? cpu->csr.mideleg : cpu->csr.medeleg;
    if (cpu->privilege <= 1 && (deleg_mask & (UINT64_C(1) << cause_low))) {
        // Execute handler in S-mode.
        cpu->csr.sepc     = trap.epc;
        cpu->csr.scause   = trap.cause;
        cpu->csr.stval    = trap.tval;
        bool spie         = cpu->csr.mstatus & (1 << RV_STATUS_SIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(1) << RV_STATUS_SPIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(1) << RV_STATUS_SIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(1) << RV_STATUS_SPP_BIT);
        cpu->csr.mstatus |= (uint64_t)spie << RV_STATUS_SPIE_BIT;
        cpu->csr.mstatus |= (uint64_t)cpu->privilege << RV_STATUS_SPP_BIT;
        cpu->privilege    = 1;
        uint64_t base     = cpu->csr.stvec & ~UINT64_C(3);
        // Vectored mode: interrupts jump to BASE + 4*cause; exceptions go to BASE.
        cpu->pc = (is_interrupt && (cpu->csr.stvec & 1)) ? base + 4 * cause_low
                                                         : base;
    } else {
        // Execute handler in M-mode.
        cpu->csr.mepc     = trap.epc;
        cpu->csr.mcause   = trap.cause;
        cpu->csr.mtval    = trap.tval;
        bool mpie         = cpu->csr.mstatus & (1 << RV_STATUS_MIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(1) << RV_STATUS_MPIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(1) << RV_STATUS_MIE_BIT);
        cpu->csr.mstatus &= ~(UINT64_C(3) << RV_STATUS_MPP_BASE_BIT);
        cpu->csr.mstatus |= (uint64_t)mpie << RV_STATUS_MPIE_BIT;
        cpu->csr.mstatus |= (uint64_t)cpu->privilege << RV_STATUS_MPP_BASE_BIT;
        cpu->privilege    = 3;
        uint64_t base     = cpu->csr.mtvec & ~UINT64_C(3);
        // Vectored mode: interrupts jump to BASE + 4*cause; exceptions go to BASE.
        cpu->pc = (is_interrupt && (cpu->csr.mtvec & 1)) ? base + 4 * cause_low
                                                         : base;
    }
}

// Execute the illegal instruction handler.
void rv_do_iillegal(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    rv_do_trap(
        machine,
        cpu,
        (struct rv_trap){
            .epc   = cpu->epc,
            .cause = RV_CAUSE_IILLEGAL,
            .tval  = insn,
        }
    );
}

// Interrupt priority order per RISC-V privileged spec section 3.1.9:
// MEI > MSI > MTI > SEI > SSI > STI.
static const int irq_priority[] = {
    RV_INTR_MEIP,
    RV_INTR_MSIP,
    RV_INTR_MTIP,
    RV_INTR_SEIP,
    RV_INTR_SSIP,
    RV_INTR_STIP,
};

// Check for pending, enabled interrupts and dispatch the highest-priority one.
void rv_check_interrupts(struct rv_machine *machine, struct rv_cpu *cpu) {
    uint64_t pending =
        atomic_load_explicit(&cpu->irq_pending, memory_order_relaxed) &
        cpu->csr.mie;
    if (!pending) return;

    uint64_t mstatus = cpu->csr.mstatus;
    uint64_t mideleg = cpu->csr.mideleg;

    // M-mode interrupts: not delegated to S-mode, taken when either running
    // below M-mode (always preemptible) or in M-mode with MIE enabled.
    uint64_t m_pending = pending & ~mideleg;
    bool m_enabled =
        (cpu->privilege < 3) || ((mstatus >> RV_STATUS_MIE_BIT) & 1);

    // S-mode interrupts: delegated to S-mode, taken when either running below
    // S-mode or in S-mode with SIE enabled.
    uint64_t s_pending = pending & mideleg;
    bool s_enabled =
        (cpu->privilege < 1) ||
        (cpu->privilege == 1 && ((mstatus >> RV_STATUS_SIE_BIT) & 1));

    // M-mode takes priority over S-mode.
    uint64_t take = 0;
    if (m_enabled && m_pending) {
        take = m_pending;
    } else if (s_enabled && s_pending) {
        take = s_pending;
    } else {
        return;
    }

    // Find highest-priority pending interrupt.
    int cause_bit = -1;
    for (int i = 0; i < (int)(sizeof irq_priority / sizeof irq_priority[0]); i++) {
        if (take & (UINT64_C(1) << irq_priority[i])) {
            cause_bit = irq_priority[i];
            break;
        }
    }
    if (cause_bit < 0) return;

    rv_do_trap(
        machine,
        cpu,
        (struct rv_trap){
            .epc   = cpu->pc,
            .cause = RV_CAUSE_INTR_FLAG | (uint64_t)cause_bit,
            .tval  = 0,
        }
    );
}
