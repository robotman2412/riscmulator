
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "cpu/rv_paging.h"

#include "cpu/rv_cpu.h"
#include "cpu/rv_csr.h"
#include "cpu/rv_pmp.h"
#include "cpu/rv_privileged.h"
#include "emu_machine.h"

#include <stddef.h>



// Check PMP and PTE access bits.
static enum rv_mem_result check_access(struct rv_cpu *cpu, struct rv_tlb_entry entry, enum rv_access mode) {
    bool user_pte = entry.pte & (1 << RV_PTE_U_BIT);
    if (cpu->mem_privilege == 0) {
        // U-mode access.
        if (!user_pte) {
            // U-mode denied access to supervisor pages.
            return RV_MEM_PAGE_FAULT;
        }
    } else {
        // S-mode or higher access.
        bool sum = cpu->csr.mstatus & (1 << RV_STATUS_SUM_BIT);
        if (user_pte && !sum) {
            // S-mode prevented from accidental user memory access.
            return RV_MEM_PAGE_FAULT;
        }
    }

    switch (mode) {
        case RV_ACCESS_INSN:
            if ((entry.pte & (1 << RV_PTE_X_BIT)) == 0) {
                return RV_MEM_PAGE_FAULT;
            } else if ((entry.vma & (1 << RV_PMPCFG_X_BIT)) == 0) {
                return RV_MEM_ACCESS_FAULT;
            }
            break;
        case RV_ACCESS_LOAD:
            if ((entry.pte & (1 << RV_PTE_R_BIT)) == 0) {
                return RV_MEM_PAGE_FAULT;
            } else if ((entry.vma & (1 << RV_PMPCFG_R_BIT)) == 0) {
                return RV_MEM_ACCESS_FAULT;
            }
            break;
        case RV_ACCESS_AMO:
        case RV_ACCESS_STORE:
            if ((entry.pte & (1 << RV_PTE_W_BIT)) == 0) {
                return RV_MEM_PAGE_FAULT;
            } else if ((entry.vma & (1 << RV_PMPCFG_W_BIT)) == 0) {
                return RV_MEM_ACCESS_FAULT;
            }
            break;
    }

    return RV_MEM_OK;
}

// Look up a page-table entry without using the TLB.
// Won't set the A/D bits if the PTE's permission bits wouldn't allow it.
enum rv_mem_result rv_paging_raw_lookup(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, enum rv_access mode
) {
    enum rv_mem_result res;
    uint64_t           setfl = 1 << RV_PTE_A_BIT;
    if (mode == RV_ACCESS_AMO || mode == RV_ACCESS_STORE) {
        setfl |= 1 << RV_PTE_D_BIT;
    }

    uint64_t satp_asid = (cpu->csr.satp & RV_SATP_ASID_MASK) >> RV_SATP_ASID_BASE_BIT;
    uint64_t vpn       = vaddr / RV_CPU_PAGE_SIZE;
    uint64_t ppn       = cpu->csr.satp & RV_SATP_PPN_MASK;

    for (int level = 2; level >= 0; level--) {
        uint64_t vpn_part = (vpn >> (9 * level)) & 0x1ff;
        uint64_t pte;
        uint64_t pte_paddr = ppn * RV_CPU_PAGE_SIZE + vpn_part * 8;

        uint8_t pte_pmp = rv_pmp_check(machine, cpu, pte_paddr, 8, false);
        if ((pte_pmp & (1 << RV_PMPCFG_R_BIT)) == 0) {
            return RV_MEM_ACCESS_FAULT;
        }
        res = rv_access_phys(machine, cpu, pte_paddr, &pte, 3, RV_ACCESS_LOAD, true);
        if (res != RV_MEM_OK) {
            return res;
        }

        if ((pte & (1 << RV_PTE_V_BIT)) == 0) {
            // Nothing mapped.
            return RV_MEM_PAGE_FAULT;
        }

        uint64_t next_ppn = (pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
        if (pte & RV_PTE_RWX_MASK) {
            // Leaf PTE.
            uint64_t align_mask = (1 << (9 * level)) - 1;
            if ((next_ppn & align_mask) != 0) {
                return RV_MEM_PAGE_FAULT;
            }

            // Cache PMP result of actual target page.
            uint8_t next_pmp = rv_pmp_check(machine, cpu, next_ppn * RV_CPU_PAGE_SIZE, RV_CPU_PAGE_SIZE, false);

            uint64_t superpage_offset = (vaddr >> RV_CPU_PAGE_SIZE_EXP) & align_mask;

            struct rv_tlb_entry entry = {
                .vma = satp_asid << RV_TLB_ASID_BASE_BIT | (vaddr & RV_TLB_VPN_MASK) | next_pmp,
                .pte = pte | superpage_offset << RV_PTE_PPN_BASE_BIT,
            };
            res = check_access(cpu, entry, mode);
            if (res != RV_MEM_OK) {
                return res;
            }
            if ((pte & setfl) != setfl) {
                if ((pte_pmp & (1 << RV_PMPCFG_W_BIT)) == 0) {
                    // Couldn't write to the PTE.
                    return RV_MEM_ACCESS_FAULT;
                }
                // Set A/D flags.
                pte       |= setfl;
                entry.pte |= setfl;
                res        = rv_access_phys(machine, cpu, pte_paddr, &pte, 3, RV_ACCESS_STORE, true);
                if (res != RV_MEM_OK) {
                    return res;
                }
            }

            // Valid leaf PTE.
            *out = entry;
            return RV_MEM_OK;
        } else {
            // Non-leaf PTE.
            ppn = next_ppn;
        }
    }

    // Non-leaf PTE at level 0 is invalid, fall through to page fault.
    return RV_MEM_PAGE_FAULT;
}

// Do a cached lookup; try reading from the TLB first.
// Won't set the A/D bits if the PTE's permission bits wouldn't allow it.
enum rv_mem_result rv_paging_lookup(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, enum rv_access mode
) {
    enum rv_mem_result res;
    size_t             row   = vaddr / RV_CPU_PAGE_SIZE % RV_TLB_ROWS;
    uint64_t           setfl = 1 << RV_PTE_A_BIT;
    if (mode == RV_ACCESS_AMO || mode == RV_ACCESS_STORE) {
        setfl |= 1 << RV_PTE_D_BIT;
    }

    uint16_t satp_asid = (cpu->csr.satp & RV_SATP_ASID_MASK) >> RV_SATP_ASID_BASE_BIT;

    // Check the TLB.
    for (size_t col = 0; col < RV_TLB_COLUMNS; col++) {
        if ((cpu->tlb.valid[row] & (1 << col)) == 0) {
            continue;
        }
        struct rv_tlb_entry const *entry = &cpu->tlb.entries[row * RV_TLB_COLUMNS + col];
        // TODO: TLB support for hugepages?
        if ((entry->vma & RV_TLB_VPN_MASK) == (vaddr & RV_TLB_VPN_MASK)) {
            uint16_t entry_asid = (entry->vma & RV_TLB_ASID_MASK) >> RV_TLB_ASID_BASE_BIT;
            if ((entry->pte & (1 << RV_PTE_G_BIT)) == 0 && entry_asid != satp_asid) {
                // Non-global entry with different ASID.
                continue;
            }
            if ((entry->pte & setfl) != setfl) {
                // Fall back to walking page table so these bits get set.
                // Also mark invalid so this (or another invalid) entry is
                // written.
                cpu->tlb.valid[row] &= ~(1 << col);
                break;
            }
            *out = *entry;
            // Valid translation found in the TLB.
            return RV_MEM_OK;
        }
    }

    // Fall back to walking the page table.
    res = rv_paging_raw_lookup(machine, cpu, vaddr, out, mode);
    if (res != RV_MEM_OK) {
        return res;
    }
    if ((out->pte & (1 << RV_PTE_V_BIT)) == 0) {
        // No faults, but invalid PTE.
        return RV_MEM_OK;
    }

    // Insert into the TLB; first, try to insert into an invalid entry.
    for (size_t col = 0; col < RV_TLB_COLUMNS; col++) {
        if ((cpu->tlb.valid[row] & (1 << col)) == 0) {
            struct rv_tlb_entry *entry  = &cpu->tlb.entries[row * RV_TLB_COLUMNS + col];
            *entry                      = *out;
            cpu->tlb.valid[row]        |= 1 << col;
            return RV_MEM_OK;
        }
    }

    // Evict an existing entry.
    struct rv_tlb_entry *entry = &cpu->tlb.entries[row * RV_TLB_COLUMNS + cpu->tlb.next_column];
    *entry                     = *out;
    cpu->tlb.next_column++;

    return RV_MEM_OK;
}

// Implementation of `rv_access_virt` for accesses spanning page boundaries.
static inline enum rv_mem_result rv_access_virt_pageboundary(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, void *data, uint8_t size_exp, enum rv_access mode
) {
    enum rv_mem_result  res;
    struct rv_tlb_entry result0;
    struct rv_tlb_entry result1;
    size_t              size   = 1 << size_exp;
    uint64_t            vaddr1 = (vaddr / RV_CPU_PAGE_SIZE + 1) * RV_CPU_PAGE_SIZE;
    size_t              size0  = vaddr1 - vaddr;
    size_t              size1  = vaddr + size - vaddr1;

    if (!rv_is_canon_vaddr(cpu, vaddr1)) {
        return RV_MEM_PAGE_FAULT;
    }

    res = rv_paging_lookup(machine, cpu, vaddr, &result0, mode);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }
    res = rv_paging_lookup(machine, cpu, vaddr1, &result1, mode);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }

    // Proceed with physical-memory accesses.
    uint64_t ppn0   = (result0.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    uint64_t paddr0 = ppn0 << RV_CPU_PAGE_SIZE_EXP | vaddr % RV_CPU_PAGE_SIZE;
    uint64_t ppn1   = (result1.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    uint64_t paddr1 = ppn1 << RV_CPU_PAGE_SIZE_EXP | vaddr1 % RV_CPU_PAGE_SIZE;

    res = rv_access_phys_partial(machine, cpu, paddr0, data, size0, mode, true);
    if (res != RV_MEM_OK) {
        return res;
    }
    res = rv_access_phys_partial(machine, cpu, paddr1, data + size0, size1, mode, true);
    if (res != RV_MEM_OK) {
        return res;
    }

    return RV_MEM_OK;
}

// Access virtual memory.
enum rv_mem_result rv_access_virt(
    struct emu_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, void *data, uint8_t size_exp, enum rv_access mode
) {
    enum rv_mem_result res;
    uint8_t            eff_priv = (mode == RV_ACCESS_INSN) ? cpu->privilege : cpu->mem_privilege;
    if (eff_priv == 3 || (cpu->csr.satp & RV_SATP_MODE_MASK) >> RV_SATP_MODE_BASE_BIT == 0) {
        // Virtual memory disabled; do physical access directly.
        return rv_access_phys(machine, cpu, vaddr, data, size_exp, mode, false);
    }
    if (!rv_is_canon_vaddr(cpu, vaddr)) {
        // Non-canonical virtual address.
        return RV_MEM_PAGE_FAULT;
    }

    struct rv_tlb_entry result;
    size_t              size   = 1 << size_exp;
    uint64_t            vaddr1 = vaddr + size - 1;

    if (vaddr / RV_CPU_PAGE_SIZE != vaddr1 / RV_CPU_PAGE_SIZE) {
        // Crossess a page boundary; two lookups needed.
        return rv_access_virt_pageboundary(machine, cpu, vaddr, data, size_exp, mode);
    }

    res = rv_paging_lookup(machine, cpu, vaddr, &result, mode);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }

    // Proceed with physical-memory access.
    uint64_t ppn   = (result.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    uint64_t paddr = ppn << RV_CPU_PAGE_SIZE_EXP | vaddr % RV_CPU_PAGE_SIZE;
    return rv_access_phys(machine, cpu, paddr, data, size_exp, mode, true);
}

// Clear the entire TLB.
void rv_flush_tlb(struct rv_cpu *cpu) {
    for (size_t row = 0; row < RV_TLB_ROWS; row++) {
        cpu->tlb.valid[row] = 0;
    }
}

// Invalidate all entries with the ASID.
void rv_inval_tlb_asid(struct rv_cpu *cpu, uint16_t asid) {
    for (size_t row = 0; row < RV_TLB_ROWS; row++) {
        for (size_t col = 0; col < RV_TLB_COLUMNS; col++) {
            uint64_t vma      = cpu->tlb.entries[row * RV_TLB_COLUMNS + col].vma;
            uint16_t vma_asid = (vma & RV_TLB_ASID_MASK) >> RV_TLB_ASID_BASE_BIT;
            if (vma_asid == asid) {
                cpu->tlb.valid[row] &= ~(1 << col);
            }
        }
    }
}

// Invalidate a specific virtual address.
void rv_inval_tlb_vaddr(struct rv_cpu *cpu, uint64_t vaddr, uint16_t asid, bool with_asid) {
    size_t   row   = vaddr / RV_CPU_PAGE_SIZE % RV_TLB_ROWS;
    uint64_t mask  = RV_TLB_VPN_MASK;
    uint64_t match = vaddr & RV_TLB_VPN_MASK;
    if (with_asid) {
        mask  |= RV_TLB_ASID_MASK;
        match |= (uint64_t)asid << RV_TLB_ASID_BASE_BIT;
    }

    for (size_t col = 0; col < RV_TLB_COLUMNS; col++) {
        uint64_t vma = cpu->tlb.entries[row * RV_TLB_COLUMNS + col].vma;
        if ((vma & mask) == match) {
            cpu->tlb.valid[row] &= ~(1 << col);
        }
    }
}

// Invalidate matching TLB entries.
void rv_inval_tlb(struct rv_cpu *cpu, uint64_t vaddr, uint16_t asid, bool with_vaddr, bool with_asid) {
    if (with_vaddr && !rv_is_canon_vaddr(cpu, vaddr)) {
        // Either paging is disabled and the TLB is already empty, or this non-canonical virtual address can't be in it.
        return;
    }
    if (with_vaddr) {
        rv_inval_tlb_vaddr(cpu, vaddr, asid, with_asid);
    } else if (with_asid) {
        rv_inval_tlb_asid(cpu, asid);
    } else {
        rv_flush_tlb(cpu);
    }
}

// Whether a virtual address is canonical.
bool rv_is_canon_vaddr(struct rv_cpu *cpu, uint64_t vaddr) {
    if ((cpu->csr.satp & RV_SATP_MODE_MASK) >> RV_SATP_MODE_BASE_BIT != 8) {
        // Virtual memory disabled or unsupported mode configured.
        return false;
    }
    int const shift       = 64 - 39;
    uint64_t  canon_vaddr = (int64_t)vaddr << shift >> shift;
    return canon_vaddr == vaddr;
}
