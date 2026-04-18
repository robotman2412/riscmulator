
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

// RISC-V major opcode stored in bits [6:2].
enum rv_op_maj {
    RV_OP_MAJ_LOAD      = 0b00000,
    RV_OP_MAJ_LOAD_FP   = 0b00001,
    RV_OP_MAJ_CUSTOM0   = 0b00010,
    RV_OP_MAJ_MISC_MEM  = 0b00011,
    RV_OP_MAJ_OP_IMM    = 0b00100,
    RV_OP_MAJ_AUIPC     = 0b00101,
    RV_OP_MAJ_OP_IMM_32 = 0b00110,
    RV_OP_MAJ_STORE     = 0b01000,
    RV_OP_MAJ_STORE_FP  = 0b01001,
    RV_OP_MAJ_CUSTOM1   = 0b01010,
    RV_OP_MAJ_AMO       = 0b01011,
    RV_OP_MAJ_OP        = 0b01100,
    RV_OP_MAJ_LUI       = 0b01101,
    RV_OP_MAJ_OP_32     = 0b01110,
    RV_OP_MAJ_MADD      = 0b10000,
    RV_OP_MAJ_MSUB      = 0b10001,
    RV_OP_MAJ_NMADD     = 0b10010,
    RV_OP_MAJ_NMSUB     = 0b10011,
    RV_OP_MAJ_OP_FP     = 0b10100,
    RV_OP_MAJ_OP_V      = 0b10101,
    RV_OP_MAJ_CUSTOM2   = 0b10110,
    RV_OP_MAJ_BRANCH    = 0b11000,
    RV_OP_MAJ_JALR      = 0b11001,
    RV_OP_MAJ_RESERVED  = 0b11010,
    RV_OP_MAJ_JAL       = 0b11011,
    RV_OP_MAJ_SYSTEM    = 0b11100,
    RV_OP_MAJ_OP_VE     = 0b11101,
    RV_OP_MAJ_CUSTOM3   = 0b11110,
};

// Read a bit-field.
#define RV_INSN_BITFIELD(insn, bitpos, bitmask)                                \
    (((insn) >> (bitpos)) & (bitmask))

// Major opcode.
#define RV_INSN_OP_MAJ(insn) RV_INSN_BITFIELD(insn, 2, 0x1f)

// First source register.
#define RV_INSN_RS1(insn)    RV_INSN_BITFIELD(insn, 15, 0x1f)
// Second source register.
#define RV_INSN_RS2(insn)    RV_INSN_BITFIELD(insn, 20, 0x1f)
// Destination register.
#define RV_INSN_RD(insn)     RV_INSN_BITFIELD(insn, 7, 0x1f)
// Funct3 field.
#define RV_INSN_FUNCT3(insn) RV_INSN_BITFIELD(insn, 12, 0x7)
// Funct7 field.
#define RV_INSN_FUNCT7(insn) RV_INSN_BITFIELD(insn, 25, 0x7f)

// The 12-bit imm field (unsigned).
#define RV_INSN_UIMM12(insn) ((uint32_t)(insn) >> 20)
// The 12-bit imm field (signed).
#define RV_INSN_IMM12(insn)  ((int32_t)(insn) >> 20)
// The 12-bit imm field for S-type instructions (signed).
#define RV_INSN_S_IMM12(insn)                                                  \
    (((int32_t)((insn) & 0xfe000000) >> 20) | ((int32_t)((insn) & 0xf80) >> 7))
