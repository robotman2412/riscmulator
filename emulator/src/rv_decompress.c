
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_decompress.h"
#include "rv_insn.h"

// Full 7-bit opcode from the 5-bit major field (all 32-bit insns have bits[1:0]=11).
#define OP(maj) (((maj) << 2) | 3)

// Compressed register: 3-bit field maps to x8–x15.
#define CREG(x) (8u + (uint32_t)(x))

// ── Quadrant 0: bits[1:0] = 00 ───────────────────────────────────────────────

static bool decompress_q0(uint16_t in, uint32_t *out) {
    uint32_t funct3 = (in >> 13) & 0x7;
    uint32_t rs1p   = CREG((in >> 7) & 0x7);
    uint32_t rdp    = CREG((in >> 2) & 0x7);  // also rs2p for stores

    switch (funct3) {
        case 0: {
            // C.ADDI4SPN → ADDI rd', x2, nzuimm
            // nzuimm[5:4]=in[12:11], nzuimm[9:6]=in[10:7], nzuimm[2]=in[6], nzuimm[3]=in[5]
            uint32_t nzuimm =
                (((in >> 11) & 0x3u) << 4) |
                (((in >>  7) & 0xfu) << 6) |
                (((in >>  6) & 0x1u) << 2) |
                (((in >>  5) & 0x1u) << 3);
            if (nzuimm == 0) return false;  // reserved
            *out = rv_enc_i((int32_t)nzuimm, 2, 0, rdp, OP(RV_OP_MAJ_OP_IMM));
            return true;
        }
        case 1: {
            // C.FLD → FLD rd', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[7:6]=in[6:5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  5) & 0x3u) << 6);
            *out = rv_enc_i((int32_t)uimm, rs1p, 3, rdp, OP(RV_OP_MAJ_LOAD_FP));
            return true;
        }
        case 2: {
            // C.LW → LW rd', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[2]=in[6], uimm[6]=in[5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  6) & 0x1u) << 2) |
                (((in >>  5) & 0x1u) << 6);
            *out = rv_enc_i((int32_t)uimm, rs1p, 2, rdp, OP(RV_OP_MAJ_LOAD));
            return true;
        }
        case 3: {
            // C.LD → LD rd', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[7:6]=in[6:5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  5) & 0x3u) << 6);
            *out = rv_enc_i((int32_t)uimm, rs1p, 3, rdp, OP(RV_OP_MAJ_LOAD));
            return true;
        }
        case 4: return false;  // reserved
        case 5: {
            // C.FSD → FSD rs2', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[7:6]=in[6:5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  5) & 0x3u) << 6);
            *out = rv_enc_s((int32_t)uimm, rdp, rs1p, 3, OP(RV_OP_MAJ_STORE_FP));
            return true;
        }
        case 6: {
            // C.SW → SW rs2', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[2]=in[6], uimm[6]=in[5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  6) & 0x1u) << 2) |
                (((in >>  5) & 0x1u) << 6);
            *out = rv_enc_s((int32_t)uimm, rdp, rs1p, 2, OP(RV_OP_MAJ_STORE));
            return true;
        }
        case 7: {
            // C.SD → SD rs2', uimm(rs1')
            // uimm[5:3]=in[12:10], uimm[7:6]=in[6:5]
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  5) & 0x3u) << 6);
            *out = rv_enc_s((int32_t)uimm, rdp, rs1p, 3, OP(RV_OP_MAJ_STORE));
            return true;
        }
    }
    return false;
}

// ── Quadrant 1: bits[1:0] = 01 ───────────────────────────────────────────────

static bool decompress_q1(uint16_t in, uint32_t *out) {
    uint32_t funct3 = (in >> 13) & 0x7;

    switch (funct3) {
        case 0: {
            // C.NOP (rd=0, nzimm=0) or C.ADDI (rd≠0)
            // nzimm[5]=in[12], nzimm[4:0]=in[6:2]
            uint32_t rd     = (in >> 7) & 0x1fu;
            int32_t  nzimm  = (int32_t)(((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu));
            nzimm = (nzimm << 26) >> 26;
            *out = rv_enc_i(nzimm, rd, 0, rd, OP(RV_OP_MAJ_OP_IMM));
            return true;
        }
        case 1: {
            // C.ADDIW → ADDIW rd, rd, nzimm  (rd≠0)
            uint32_t rd    = (in >> 7) & 0x1fu;
            if (rd == 0) return false;  // reserved
            int32_t  nzimm = (int32_t)(((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu));
            nzimm = (nzimm << 26) >> 26;
            *out = rv_enc_i(nzimm, rd, 0, rd, OP(RV_OP_MAJ_OP_IMM_32));
            return true;
        }
        case 2: {
            // C.LI → ADDI rd, x0, imm
            uint32_t rd  = (in >> 7) & 0x1fu;
            int32_t  imm = (int32_t)(((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu));
            imm = (imm << 26) >> 26;
            *out = rv_enc_i(imm, 0, 0, rd, OP(RV_OP_MAJ_OP_IMM));
            return true;
        }
        case 3: {
            uint32_t rd = (in >> 7) & 0x1fu;
            if (rd == 2) {
                // C.ADDI16SP → ADDI sp, sp, nzimm
                // nzimm[9]=in[12], nzimm[4]=in[6], nzimm[6]=in[5],
                // nzimm[8:7]=in[4:3], nzimm[5]=in[2]
                int32_t nzimm =
                    (int32_t)(
                        (((in >> 12) & 0x1u) << 9) |
                        (((in >>  6) & 0x1u) << 4) |
                        (((in >>  5) & 0x1u) << 6) |
                        (((in >>  3) & 0x3u) << 7) |
                        (((in >>  2) & 0x1u) << 5));
                nzimm = (nzimm << 22) >> 22;
                if (nzimm == 0) return false;  // reserved
                *out = rv_enc_i(nzimm, 2, 0, 2, OP(RV_OP_MAJ_OP_IMM));
                return true;
            } else if (rd != 0) {
                // C.LUI → LUI rd, nzimm
                // nzimm_6bit: [5]=in[12], [4:0]=in[6:2]
                int32_t nzimm = (int32_t)(((in >> 12) & 0x1u) << 5) |
                                (int32_t)((in >> 2) & 0x1fu);
                nzimm = (nzimm << 26) >> 26;
                if (nzimm == 0) return false;  // reserved
                *out = rv_enc_u(nzimm, rd, OP(RV_OP_MAJ_LUI));
                return true;
            }
            return false;  // rd=0: reserved
        }
        case 4: {
            // C.SRLI / C.SRAI / C.ANDI / C.SUB / C.XOR / C.OR / C.AND / C.SUBW / C.ADDW
            uint32_t funct2 = (in >> 10) & 0x3u;
            uint32_t rdp    = CREG((in >> 7) & 0x7u);

            switch (funct2) {
                case 0: {
                    // C.SRLI → SRLI rd', rd', shamt
                    uint32_t shamt = ((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu);
                    *out = rv_enc_i((int32_t)(shamt & 0x3fu), rdp, 5, rdp,
                                   OP(RV_OP_MAJ_OP_IMM));
                    return true;
                }
                case 1: {
                    // C.SRAI → SRAI rd', rd', shamt  (bit10 of imm set)
                    uint32_t shamt = ((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu);
                    *out = rv_enc_i((int32_t)((1u << 10) | (shamt & 0x3fu)), rdp, 5, rdp,
                                   OP(RV_OP_MAJ_OP_IMM));
                    return true;
                }
                case 2: {
                    // C.ANDI → ANDI rd', rd', imm
                    int32_t imm = (int32_t)(((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu));
                    imm = (imm << 26) >> 26;
                    *out = rv_enc_i(imm, rdp, 7, rdp, OP(RV_OP_MAJ_OP_IMM));
                    return true;
                }
                case 3: {
                    uint32_t rs2p  = CREG((in >> 2) & 0x7u);
                    uint32_t op2   = (in >> 5) & 0x3u;
                    bool     is_w  = (in >> 12) & 0x1u;

                    if (!is_w) {
                        switch (op2) {
                            case 0: *out = rv_enc_r(0x20, rs2p, rdp, 0, rdp, OP(RV_OP_MAJ_OP)); return true;  // C.SUB
                            case 1: *out = rv_enc_r(0x00, rs2p, rdp, 4, rdp, OP(RV_OP_MAJ_OP)); return true;  // C.XOR
                            case 2: *out = rv_enc_r(0x00, rs2p, rdp, 6, rdp, OP(RV_OP_MAJ_OP)); return true;  // C.OR
                            case 3: *out = rv_enc_r(0x00, rs2p, rdp, 7, rdp, OP(RV_OP_MAJ_OP)); return true;  // C.AND
                        }
                    } else {
                        switch (op2) {
                            case 0: *out = rv_enc_r(0x20, rs2p, rdp, 0, rdp, OP(RV_OP_MAJ_OP_32)); return true;  // C.SUBW
                            case 1: *out = rv_enc_r(0x00, rs2p, rdp, 0, rdp, OP(RV_OP_MAJ_OP_32)); return true;  // C.ADDW
                            default: return false;  // reserved
                        }
                    }
                    return false;
                }
            }
            return false;
        }
        case 5: {
            // C.J → JAL x0, offset
            // offset[11]=in[12], offset[4]=in[11], offset[9:8]=in[10:9],
            // offset[10]=in[8], offset[6]=in[7], offset[7]=in[6],
            // offset[3:1]=in[5:3], offset[5]=in[2]
            int32_t offset =
                (int32_t)(
                    (((in >> 12) & 0x1u) << 11) |
                    (((in >> 11) & 0x1u) <<  4) |
                    (((in >>  9) & 0x3u) <<  8) |
                    (((in >>  8) & 0x1u) << 10) |
                    (((in >>  7) & 0x1u) <<  6) |
                    (((in >>  6) & 0x1u) <<  7) |
                    (((in >>  3) & 0x7u) <<  1) |
                    (((in >>  2) & 0x1u) <<  5));
            offset = (offset << 20) >> 20;
            *out = rv_enc_j(offset, 0, OP(RV_OP_MAJ_JAL));
            return true;
        }
        case 6: {
            // C.BEQZ → BEQ rs1', x0, offset
            // offset[8]=in[12], offset[4:3]=in[11:10], offset[7:6]=in[6:5],
            // offset[2:1]=in[4:3], offset[5]=in[2]
            uint32_t rs1p = CREG((in >> 7) & 0x7u);
            int32_t  offset =
                (int32_t)(
                    (((in >> 12) & 0x1u) << 8) |
                    (((in >> 10) & 0x3u) << 3) |
                    (((in >>  5) & 0x3u) << 6) |
                    (((in >>  3) & 0x3u) << 1) |
                    (((in >>  2) & 0x1u) << 5));
            offset = (offset << 23) >> 23;
            *out = rv_enc_b(offset, 0, rs1p, 0, OP(RV_OP_MAJ_BRANCH));
            return true;
        }
        case 7: {
            // C.BNEZ → BNE rs1', x0, offset  (same encoding as BEQZ)
            uint32_t rs1p = CREG((in >> 7) & 0x7u);
            int32_t  offset =
                (int32_t)(
                    (((in >> 12) & 0x1u) << 8) |
                    (((in >> 10) & 0x3u) << 3) |
                    (((in >>  5) & 0x3u) << 6) |
                    (((in >>  3) & 0x3u) << 1) |
                    (((in >>  2) & 0x1u) << 5));
            offset = (offset << 23) >> 23;
            *out = rv_enc_b(offset, 0, rs1p, 1, OP(RV_OP_MAJ_BRANCH));
            return true;
        }
    }
    return false;
}

// ── Quadrant 2: bits[1:0] = 10 ───────────────────────────────────────────────

static bool decompress_q2(uint16_t in, uint32_t *out) {
    uint32_t funct3 = (in >> 13) & 0x7;

    switch (funct3) {
        case 0: {
            // C.SLLI → SLLI rd, rd, shamt
            uint32_t rd    = (in >> 7) & 0x1fu;
            uint32_t shamt = ((in >> 7) & 0x20u) | ((in >> 2) & 0x1fu);
            if (rd == 0) return false;  // HINT
            *out = rv_enc_i((int32_t)(shamt & 0x3fu), rd, 1, rd, OP(RV_OP_MAJ_OP_IMM));
            return true;
        }
        case 1: {
            // C.FLDSP → FLD rd, uimm(x2)
            // uimm[5]=in[12], uimm[4:3]=in[6:5], uimm[8:6]=in[4:2]
            uint32_t rd   = (in >> 7) & 0x1fu;
            uint32_t uimm =
                (((in >> 12) & 0x1u) << 5) |
                (((in >>  5) & 0x3u) << 3) |
                (((in >>  2) & 0x7u) << 6);
            *out = rv_enc_i((int32_t)uimm, 2, 3, rd, OP(RV_OP_MAJ_LOAD_FP));
            return true;
        }
        case 2: {
            // C.LWSP → LW rd, uimm(x2)
            // uimm[5]=in[12], uimm[4:2]=in[6:4], uimm[7:6]=in[3:2]
            uint32_t rd   = (in >> 7) & 0x1fu;
            if (rd == 0) return false;  // reserved
            uint32_t uimm =
                (((in >> 12) & 0x1u) << 5) |
                (((in >>  4) & 0x7u) << 2) |
                (((in >>  2) & 0x3u) << 6);
            *out = rv_enc_i((int32_t)uimm, 2, 2, rd, OP(RV_OP_MAJ_LOAD));
            return true;
        }
        case 3: {
            // C.LDSP → LD rd, uimm(x2)
            // uimm[5]=in[12], uimm[4:3]=in[6:5], uimm[8:6]=in[4:2]
            uint32_t rd   = (in >> 7) & 0x1fu;
            if (rd == 0) return false;  // reserved
            uint32_t uimm =
                (((in >> 12) & 0x1u) << 5) |
                (((in >>  5) & 0x3u) << 3) |
                (((in >>  2) & 0x7u) << 6);
            *out = rv_enc_i((int32_t)uimm, 2, 3, rd, OP(RV_OP_MAJ_LOAD));
            return true;
        }
        case 4: {
            // C.JR / C.MV / C.EBREAK / C.JALR / C.ADD
            uint32_t rs1_rd = (in >> 7) & 0x1fu;
            uint32_t rs2    = (in >> 2) & 0x1fu;
            bool     bit12  = (in >> 12) & 0x1u;

            if (!bit12) {
                if (rs2 == 0) {
                    if (rs1_rd == 0) return false;  // reserved
                    // C.JR → JALR x0, 0(rs1)
                    *out = rv_enc_i(0, rs1_rd, 0, 0, OP(RV_OP_MAJ_JALR));
                } else {
                    // C.MV → ADD rd, x0, rs2
                    *out = rv_enc_r(0, rs2, 0, 0, rs1_rd, OP(RV_OP_MAJ_OP));
                }
            } else {
                if (rs2 == 0 && rs1_rd == 0) {
                    // C.EBREAK → EBREAK (SYSTEM opcode, imm=1)
                    *out = (1u << 20) | OP(RV_OP_MAJ_SYSTEM);
                } else if (rs2 == 0) {
                    // C.JALR → JALR x1, 0(rs1)
                    *out = rv_enc_i(0, rs1_rd, 0, 1, OP(RV_OP_MAJ_JALR));
                } else {
                    // C.ADD → ADD rd, rd, rs2
                    *out = rv_enc_r(0, rs2, rs1_rd, 0, rs1_rd, OP(RV_OP_MAJ_OP));
                }
            }
            return true;
        }
        case 5: {
            // C.FSDSP → FSD rs2, uimm(x2)
            // uimm[5:3]=in[12:10], uimm[8:6]=in[9:7]
            uint32_t rs2  = (in >> 2) & 0x1fu;
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  7) & 0x7u) << 6);
            *out = rv_enc_s((int32_t)uimm, rs2, 2, 3, OP(RV_OP_MAJ_STORE_FP));
            return true;
        }
        case 6: {
            // C.SWSP → SW rs2, uimm(x2)
            // uimm[5:2]=in[12:9], uimm[7:6]=in[8:7]
            uint32_t rs2  = (in >> 2) & 0x1fu;
            uint32_t uimm =
                (((in >> 9) & 0xfu) << 2) |
                (((in >> 7) & 0x3u) << 6);
            *out = rv_enc_s((int32_t)uimm, rs2, 2, 2, OP(RV_OP_MAJ_STORE));
            return true;
        }
        case 7: {
            // C.SDSP → SD rs2, uimm(x2)
            // uimm[5:3]=in[12:10], uimm[8:6]=in[9:7]
            uint32_t rs2  = (in >> 2) & 0x1fu;
            uint32_t uimm =
                (((in >> 10) & 0x7u) << 3) |
                (((in >>  7) & 0x7u) << 6);
            *out = rv_enc_s((int32_t)uimm, rs2, 2, 3, OP(RV_OP_MAJ_STORE));
            return true;
        }
    }
    return false;
}

// ── Public entry point ────────────────────────────────────────────────────────

bool rv_decompress(uint16_t in, uint32_t *out) {
    switch (in & 0x3) {
        case 0: return decompress_q0(in, out);
        case 1: return decompress_q1(in, out);
        case 2: return decompress_q2(in, out);
        default: return false;  // bits[1:0]=11 → not a compressed instruction
    }
}
