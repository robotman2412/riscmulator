
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_privileged.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"

#include <assert.h>

// Execute a certain trap handler.
void rv_do_trap(struct rv_machine *machine, struct rv_cpu *cpu, struct rv_trap trap) {
    assert(trap.cause < 32);
    if (machine->trap_hook[trap.cause] && !machine->trap_hook[trap.cause](machine->hook_cookie, machine, cpu, trap)) {
        // Trap was handled by external hook.
        return;
    }

    if (cpu->privilege <= 1 && (cpu->csr.medeleg & (1 << trap.cause))) {
        // Execute handler in S-mode.
        cpu->csr.sepc     = trap.epc;
        cpu->csr.scause   = trap.cause;
        cpu->csr.stval    = trap.tval;
        bool spie         = cpu->csr.mstatus & (1 << RV_STATUS_SIE_BIT);
        cpu->csr.mstatus &= ~(1 << RV_STATUS_SPIE_BIT);
        cpu->csr.mstatus &= ~(1 << RV_STATUS_SIE_BIT);
        cpu->csr.mstatus &= ~(1 << RV_STATUS_SPP_BIT);
        cpu->csr.mstatus |= spie << RV_STATUS_SPIE_BIT;
        cpu->csr.mstatus |= cpu->privilege << RV_STATUS_SPP_BIT;
        cpu->privilege    = 1;
        cpu->pc           = cpu->csr.stvec;
    } else {
        // Execute handler in M-mode.
        cpu->csr.mepc     = trap.epc;
        cpu->csr.mcause   = trap.cause;
        cpu->csr.mtval    = trap.tval;
        bool mpie         = cpu->csr.mstatus & (1 << RV_STATUS_MIE_BIT);
        cpu->csr.mstatus &= ~(1 << RV_STATUS_MPIE_BIT);
        cpu->csr.mstatus &= ~(1 << RV_STATUS_MIE_BIT);
        cpu->csr.mstatus &= ~(3 << RV_STATUS_MPP_BASE_BIT);
        cpu->csr.mstatus |= mpie << RV_STATUS_MPIE_BIT;
        cpu->csr.mstatus |= cpu->privilege << RV_STATUS_MPP_BASE_BIT;
        cpu->privilege    = 3;
        cpu->pc           = cpu->csr.mtvec;
    }
}

// Execute the illegal instruction handler.
void rv_do_iillegal(struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn) {
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
