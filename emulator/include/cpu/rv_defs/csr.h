
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#ifndef RV_CSR_DEF
#define RV_CSR_DEF(index, name)
#endif

/* ==== FLOATING-POINT STATUS ==== */
RV_CSR_DEF(0x001, fflags)
RV_CSR_DEF(0x002, frm)
RV_CSR_DEF(0x003, fcsr)

/* ==== VECTOR STATUS ==== */
RV_CSR_DEF(0x008, vstart)
RV_CSR_DEF(0x009, vxsat)
RV_CSR_DEF(0x00A, vxrm)
RV_CSR_DEF(0x00F, vcsr)
RV_CSR_DEF(0xC20, vl)
RV_CSR_DEF(0xC21, vtype)
RV_CSR_DEF(0xC22, vlenb)

/* ==== COUNTERS ==== */
RV_CSR_DEF(0xC00, cycle)
RV_CSR_DEF(0xC01, time)
RV_CSR_DEF(0xC02, instret)

/* ==== SUPERVISOR MODE ==== */
RV_CSR_DEF(0x100, sstatus)
RV_CSR_DEF(0x104, sie)
RV_CSR_DEF(0x105, stvec)
RV_CSR_DEF(0x140, sscratch)
RV_CSR_DEF(0x141, sepc)
RV_CSR_DEF(0x142, scause)
RV_CSR_DEF(0x143, stval)
RV_CSR_DEF(0x144, sip)
RV_CSR_DEF(0x180, satp)

/* ==== MACHINE MODE ==== */
RV_CSR_DEF(0xF11, mvendorid)
RV_CSR_DEF(0xF12, marchid)
RV_CSR_DEF(0xF13, mimpid)
RV_CSR_DEF(0xF14, mhartid)
RV_CSR_DEF(0xF15, mconfigptr)
RV_CSR_DEF(0x300, mstatus)
RV_CSR_DEF(0x301, misa)
RV_CSR_DEF(0x302, medeleg)
RV_CSR_DEF(0x303, mideleg)
RV_CSR_DEF(0x304, mie)
RV_CSR_DEF(0x305, mtvec)
RV_CSR_DEF(0x306, mcounteren)
RV_CSR_DEF(0x340, mscratch)
RV_CSR_DEF(0x341, mepc)
RV_CSR_DEF(0x342, mcause)
RV_CSR_DEF(0x343, mtval)
RV_CSR_DEF(0x344, mip)
RV_CSR_DEF(0x34A, mtinst)

/* ==== PHYSICAL MEMORY PROTECTION ==== */
RV_CSR_DEF(0x3A0, pmpcfg0)
RV_CSR_DEF(0x3AF, pmpcfg15)
RV_CSR_DEF(0x3B0, pmpaddr0)
RV_CSR_DEF(0x3EF, pmpaddr63)

/* ==== MACHINE MODE COUNTERS ==== */
RV_CSR_DEF(0xB00, mcycle)
RV_CSR_DEF(0xB02, minstret)

#undef RV_CSR_DEF
