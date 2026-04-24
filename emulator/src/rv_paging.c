
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_paging.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_machine.h"
#include "rv_pmp.h"
#include "rv_privileged.h"



// Look up a page-table entry without using the TLB.
// Won't set the A/D bits if the PTE's permission bits wouldn't allow it.
enum rv_mem_result rv_paging_raw_lookup(
    struct rv_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, bool set_a, bool set_d
) {
    enum rv_mem_result res;
    uint64_t           setfl = 0;
    if (set_d) {
        setfl = 1 << RV_PTE_A_BIT | 1 << RV_PTE_D_BIT;
    } else if (set_a) {
        setfl = 1 << RV_PTE_A_BIT;
    }

    uint64_t satp_asid = (cpu->csr.satp & RV_SATP_ASID_MASK) << RV_SATP_ASID_BASE_BIT;
    uint64_t vpn       = vaddr / RV_CPU_PAGE_SIZE;
    uint64_t ppn       = cpu->csr.satp & RV_SATP_PPN_MASK;

    for (int level = 2; level >= 0; level--) {
        uint64_t vpn_part = (vpn >> (9 * (2 - level))) & 0x1ff;
        uint64_t pte;
        uint64_t pte_paddr = ppn * RV_CPU_PAGE_SIZE + vpn_part * 8;

        uint8_t pte_pmp = rv_pmp_check(machine, cpu, pte_paddr, 8, false);
        if ((pte_pmp & RV_PMPCFG_R_BIT) == 0) {
            return RV_MEM_ACCESS_FAULT;
        }
        res = rv_access_phys(machine, cpu, pte_paddr, &pte, 3, RV_ACCESS_LOAD, true);
        if (res != RV_MEM_OK) {
            return res;
        }

        if ((pte & (1 << RV_PTE_V_BIT)) == 0) {
            // Nothing mapped.
            *out = (struct rv_tlb_entry){0};
            return true;
        }

        uint64_t next_ppn = (pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
        if (pte & RV_PTE_RWX_MASK) {
            // Leaf PTE.
            uint64_t align_mask = (1 << (9 * level)) - 1;
            if ((next_ppn & align_mask) != 0) {
                return RV_MEM_PAGE_FAULT;
            }
            if ((pte & setfl) != setfl) {
                if ((pte_pmp & RV_PMPCFG_W_BIT) == 0) {
                    // Couldn't write to the PTE.
                    return RV_MEM_ACCESS_FAULT;
                }
                // Set A/D flags.
                pte |= setfl;
                res  = rv_access_phys(machine, cpu, pte_paddr, &pte, 3, RV_ACCESS_STORE, true);
                if (res != RV_MEM_OK) {
                    return res;
                }
            }

            // Cache PMP result of actual target page.
            uint8_t next_pmp = rv_pmp_check(machine, cpu, next_ppn * RV_CPU_PAGE_SIZE, RV_CPU_PAGE_SIZE, false);

            // Valid leaf PTE.
            out->vma = satp_asid << RV_TLB_ASID_BASE_BIT | (vaddr & RV_TLB_VPN_MASK) | next_pmp;
            out->pte = pte;
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
    struct rv_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, struct rv_tlb_entry *out, bool set_a, bool set_d
) {
    enum rv_mem_result res;
    size_t             row = vaddr / RV_CPU_PAGE_SIZE % RV_TLB_ROWS;

    uint64_t setfl = 0;
    if (set_d) {
        setfl = 1 << RV_PTE_A_BIT | 1 << RV_PTE_D_BIT;
    } else if (set_a) {
        setfl = 1 << RV_PTE_A_BIT;
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
            if ((entry->pte & RV_PTE_G_BIT) == 0 && entry_asid != satp_asid) {
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
    res = rv_paging_raw_lookup(machine, cpu, vaddr, out, set_a, set_d);
    if (res != RV_MEM_OK) {
        return res;
    }
    if ((out->pte & RV_PTE_V_BIT) == 0) {
        // No faults, but invalid PTE.
        return RV_MEM_OK;
    }

    // Insert into the TLB; first, try to insert into an invalid entry.
    for (size_t col = 0; col < RV_TLB_COLUMNS; col++) {
        if ((cpu->tlb.valid[row] & (1 << col)) == 0) {
            struct rv_tlb_entry *entry  = &cpu->tlb.entries[row * RV_TLB_COLUMNS + col];
            *entry                      = *out;
            cpu->tlb.valid[row]        |= 1 << col;
            return true;
        }
    }

    // Evict an existing entry.
    struct rv_tlb_entry *entry = &cpu->tlb.entries[row * RV_TLB_COLUMNS + cpu->tlb.next_column];
    *entry                     = *out;
    cpu->tlb.next_column++;

    return RV_MEM_OK;
}

// Check PMP and PTE access bits.
static enum rv_mem_result check_access(struct rv_tlb_entry entry, enum rv_access mode) {
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
    __builtin_unreachable();
}

// Implementation of `rv_access_virt` for accesses spanning page boundaries.
static inline enum rv_mem_result rv_access_virt_pageboundary(
    struct rv_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, void *data, uint8_t size_exp, enum rv_access mode
) {
    enum rv_mem_result  res;
    struct rv_tlb_entry result0;
    struct rv_tlb_entry result1;
    size_t              size   = 1 << size_exp;
    uint64_t            vaddr1 = vaddr / RV_CPU_PAGE_SIZE * RV_CPU_PAGE_SIZE + 1;
    size_t              size0  = vaddr1 - vaddr;
    size_t              size1  = vaddr + size - vaddr1;

    if (!rv_is_canon_vaddr(cpu, vaddr1)) {
        return RV_MEM_PAGE_FAULT;
    }

    res = rv_paging_lookup(machine, cpu, vaddr, &result0, true, mode == RV_ACCESS_AMO || mode == RV_ACCESS_STORE);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }
    res = rv_paging_lookup(machine, cpu, vaddr1, &result1, true, mode == RV_ACCESS_AMO || mode == RV_ACCESS_STORE);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }
    res = check_access(result0, mode);
    if (res != RV_MEM_OK) {
        // TLB does NOT grant permission.
        return res;
    }
    res = check_access(result1, mode);
    if (res != RV_MEM_OK) {
        // TLB does NOT grant permission.
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

    return true;
}

// Access virtual memory.
enum rv_mem_result rv_access_virt(
    struct rv_machine *machine, struct rv_cpu *cpu, uint64_t vaddr, void *data, uint8_t size_exp, enum rv_access mode
) {
    enum rv_mem_result res;
    if (cpu->privilege == 3 || (cpu->csr.satp & RV_SATP_MODE_MASK) >> RV_SATP_MODE_BASE_BIT == 0) {
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

    if (vaddr % RV_CPU_PAGE_SIZE != vaddr1 % RV_CPU_PAGE_SIZE) {
        // Crossess a page boundary; two lookups needed.
        return rv_access_virt_pageboundary(machine, cpu, vaddr, data, size_exp, mode);
    }

    res = rv_paging_lookup(machine, cpu, vaddr, &result, true, mode == RV_ACCESS_AMO || mode == RV_ACCESS_STORE);
    if (res != RV_MEM_OK) {
        // Faulted while translating.
        return res;
    }
    res = check_access(result, mode);
    if (res != RV_MEM_OK) {
        // TLB does NOT grant permission.
        return res;
    }

    // Proceed with physical-memory access.
    uint64_t ppn   = (result.pte & RV_PTE_PPN_MASK) >> RV_PTE_PPN_BASE_BIT;
    uint64_t paddr = ppn << RV_CPU_PAGE_SIZE_EXP | vaddr % RV_CPU_PAGE_SIZE;
    return rv_access_phys(machine, cpu, paddr, data, size_exp, mode, true);
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
