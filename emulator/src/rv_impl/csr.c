
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/csr.h"

#include "rv_cpu.h"
#include "rv_csr.h"

// Try to read a CSR; fails if no permission or does not exist.
bool rv_csr_read(struct rv_cpu *cpu, uint32_t index, uint64_t *rdata) {
    uint32_t csr_priv = (index >> 8) & 3;
    if (csr_priv > cpu->privilege) {
        return false;
    }

    switch (index) {
        case RV_CSR_mhartid: *rdata = cpu->csr.mhartid; break;
        case RV_CSR_marchid: *rdata = cpu->csr.marchid; break;
        case RV_CSR_mimpid: *rdata = cpu->csr.mimpid; break;
        case RV_CSR_mstatus: *rdata = cpu->csr.mstatus; break;
        case RV_CSR_mie: *rdata = cpu->csr.mie; break;
        case RV_CSR_mip: *rdata = cpu->csr.mip; break;
        case RV_CSR_mideleg: *rdata = cpu->csr.mideleg; break;
        case RV_CSR_medeleg: *rdata = cpu->csr.medeleg; break;
        case RV_CSR_mtvec: *rdata = cpu->csr.mtvec; break;
        case RV_CSR_mcause: *rdata = cpu->csr.mcause; break;
        case RV_CSR_mtval: *rdata = cpu->csr.mtval; break;
        case RV_CSR_mepc: *rdata = cpu->csr.mepc; break;
        case RV_CSR_mtinst: *rdata = cpu->csr.mtinst; break;
        case RV_CSR_mscratch: *rdata = cpu->csr.mscratch; break;

        case RV_CSR_sstatus: *rdata = cpu->csr.mstatus & RV_SSTATUS_MASK; break;
        case RV_CSR_sie: *rdata = cpu->csr.sie; break;
        case RV_CSR_sip: *rdata = cpu->csr.sip; break;
        case RV_CSR_stvec: *rdata = cpu->csr.stvec; break;
        case RV_CSR_scause: *rdata = cpu->csr.scause; break;
        case RV_CSR_stval: *rdata = cpu->csr.stval; break;
        case RV_CSR_sepc: *rdata = cpu->csr.sepc; break;
        case RV_CSR_sscratch: *rdata = cpu->csr.sscratch; break;
        case RV_CSR_satp: *rdata = cpu->csr.satp; break;

        case RV_CSR_fcsr:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            *rdata = cpu->csr.fcsr;
            break;
        case RV_CSR_fflags:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            *rdata = cpu->csr.fcsr & RV_FFLAGS_MASK;
            break;
        case RV_CSR_frm:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            *rdata = (cpu->csr.fcsr >> RV_FCSR_FRM_BASE_BIT) & RV_FRM_MASK;
            break;

        default: return false;
    }

    return true;
}

// Try to write a CSR; fails if no permission or does not exist.
// CSR writes do not fail for invalid values; instead, no action is taken.
bool rv_csr_write(struct rv_cpu *cpu, uint32_t index, uint64_t wdata) {
    uint32_t csr_priv = (index >> 8) & 3;
    if (csr_priv > cpu->privilege) {
        return false;
    }

    switch (index) {
        case RV_CSR_mhartid:
        case RV_CSR_marchid:
        case RV_CSR_mimpid: break; // Writes ignored.
        case RV_CSR_mstatus:
            // Ensures the value 2 never gets written to MPP.
            wdata            |= (wdata >> 1) & RV_STATUS_MPP_BASE_BIT;
            cpu->csr.mstatus  = wdata & RV_MSTATUS_MASK;
            break;
        case RV_CSR_mie: cpu->csr.mie = wdata; break;
        case RV_CSR_mip: cpu->csr.mip = wdata; break;
        case RV_CSR_mideleg: cpu->csr.mideleg = wdata; break;
        case RV_CSR_medeleg: cpu->csr.medeleg = wdata; break;
        case RV_CSR_mtvec: cpu->csr.mtvec = wdata; break;
        case RV_CSR_mcause: cpu->csr.mcause = wdata; break;
        case RV_CSR_mtval: cpu->csr.mtval = wdata; break;
        case RV_CSR_mepc: cpu->csr.mepc = wdata; break;
        case RV_CSR_mtinst: cpu->csr.mtinst = wdata; break;
        case RV_CSR_mscratch: cpu->csr.mscratch = wdata; break;

        case RV_CSR_sstatus:
            // Mask out the things supervisor may write in the status register.
            cpu->csr.mstatus &= ~RV_SSTATUS_MASK;
            cpu->csr.mstatus |= wdata & RV_SSTATUS_MASK;
            break;
        case RV_CSR_sie: cpu->csr.sie = wdata; break;
        case RV_CSR_sip: cpu->csr.sip = wdata; break;
        case RV_CSR_stvec: cpu->csr.stvec = wdata; break;
        case RV_CSR_scause: cpu->csr.scause = wdata; break;
        case RV_CSR_stval: cpu->csr.stval = wdata; break;
        case RV_CSR_sepc: cpu->csr.sepc = wdata; break;
        case RV_CSR_sscratch: cpu->csr.sscratch = wdata; break;
        case RV_CSR_satp:
            // TODO: Notify other virtual-memory structures as needed (e.g.
            // ASID, root PPN).
            cpu->csr.satp = wdata;
            break;

        case RV_CSR_fcsr:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            cpu->csr.fcsr = wdata & RV_FCSR_MASK;
            break;
        case RV_CSR_fflags:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            cpu->csr.fcsr &= ~RV_FFLAGS_MASK;
            cpu->csr.fcsr |= wdata & RV_FFLAGS_MASK;
            break;
        case RV_CSR_frm:
            if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
                return false;
            }
            cpu->csr.fcsr &= ~(RV_FRM_MASK << RV_FCSR_FRM_BASE_BIT);
            cpu->csr.fcsr |= (wdata & RV_FRM_MASK) << RV_FCSR_FRM_BASE_BIT;
            break;

        default: return false;
    }

    return true;
}
