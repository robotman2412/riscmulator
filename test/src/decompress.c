
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "cpu/rv_decompress.h"
#include "testcase.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"

// Assert that rv_decompress(in) succeeds and equals expected.
#define DECOMP_OK(in, expected)                                                                                        \
    {                                                                                                                  \
        uint32_t out_ = 0;                                                                                             \
        TEST_ASSERT(rv_decompress((in), &out_));                                                                       \
        TEST_ASSERT(out_ == (uint32_t)(expected));                                                                     \
    }

// Assert that rv_decompress(in) returns false (illegal encoding).
#define DECOMP_ILLEGAL(in)                                                                                             \
    {                                                                                                                  \
        uint32_t out_ = 0;                                                                                             \
        TEST_ASSERT(!rv_decompress((in), &out_));                                                                      \
    }

// ── Quadrant 0 ───────────────────────────────────────────────────────────────

TESTCASE(decomp_q0_addi4spn, {
    // C.ADDI4SPN: rd'=x8(000), nzuimm=4  (uimm[2]=1 → in[6]=1)
    // bits: funct3=000 | in[12:11]=00 | in[10:7]=0000 | in[6]=1 | in[5]=0 | rd'=000 | 00
    //   → 0000 0000 0100 0000 = 0x0040
    DECOMP_OK(0x0040, rv_enc_i(4, 2, 0, 8, 0x13));

    // nzuimm=0 is reserved.
    DECOMP_ILLEGAL(0x0000);
})

TESTCASE(decomp_q0_lw, {
    // C.LW: rd'=x10(010), rs1'=x14(110), offset=4  (uimm[2]=1 → in[6]=1)
    // bits: 010 | 000 | 110 | 1 | 0 | 010 | 00 → 0x4348
    DECOMP_OK(0x4348, rv_enc_i(4, 14, 2, 10, 0x03));
})

TESTCASE(decomp_q0_ld, {
    // C.LD: rd'=x8(000), rs1'=x8(000), offset=8  (uimm[3]=1 → in[10]=1)
    // bits: 011 | 001 | 000 | 00 | 000 | 00 → 0x6400
    DECOMP_OK(0x6400, rv_enc_i(8, 8, 3, 8, 0x03));
})

TESTCASE(decomp_q0_sw, {
    // C.SW: rs2'=x9(001), rs1'=x8(000), offset=4  (uimm[2]=1 → in[6]=1)
    // bits: 110 | 000 | 000 | 1 | 0 | 001 | 00 → 0xC044
    DECOMP_OK(0xC044, rv_enc_s(4, 9, 8, 2, 0x23));
})

TESTCASE(decomp_q0_sd, {
    // C.SD: rs2'=x9(001), rs1'=x8(000), offset=8  (uimm[3]=1 → in[10]=1)
    // bits: 111 | 001 | 000 | 00 | 001 | 00 → 0xE404
    DECOMP_OK(0xE404, rv_enc_s(8, 9, 8, 3, 0x23));
})

// ── Quadrant 1 ───────────────────────────────────────────────────────────────

TESTCASE(decomp_q1_addi, {
    // C.ADDI: rd=x15, nzimm=-1  (in[12]=1, in[6:2]=11111)
    // bits: 000 | 1 | 01111 | 11111 | 01 → 0x17FD
    DECOMP_OK(0x17FD, rv_enc_i(-1, 15, 0, 15, 0x13));

    // C.NOP (rd=0, nzimm=0) → ADDI x0, x0, 0
    // bits: 000 | 0 | 00000 | 00000 | 01 → 0x0001
    DECOMP_OK(0x0001, rv_enc_i(0, 0, 0, 0, 0x13));
})

TESTCASE(decomp_q1_addiw, {
    // C.ADDIW: rd=x1, nzimm=1  (in[12]=0, in[6:2]=00001)
    // bits: 001 | 0 | 00001 | 00001 | 01 → 0x2085
    DECOMP_OK(0x2085, rv_enc_i(1, 1, 0, 1, 0x1B));

    // rd=0 is reserved (bits: 001 | 0 | 00000 | 00000 | 01 → 0x2001).
    DECOMP_ILLEGAL(0x2001);
})

TESTCASE(decomp_q1_li, {
    // C.LI: rd=x1, imm=16  (in[12]=0, in[6]=1 for imm[4])
    // bits: 010 | 0 | 00001 | 10000 | 01
    //   bit7=1(rd lsb), bit6=1(imm[4]) → 0100 0000 1100 0001 = 0x40C1
    DECOMP_OK(0x40C1, rv_enc_i(16, 0, 0, 1, 0x13));
})

TESTCASE(decomp_q1_addi16sp, {
    // C.ADDI16SP: nzimm=32  (nzimm[5]=1 → in[2]=1)
    // bits: 011 | 0 | 00010 | 0|0|00|1 | 01 → 0x6105
    DECOMP_OK(0x6105, rv_enc_i(32, 2, 0, 2, 0x13));

    // nzimm=0 is reserved (bits: 011 | 0 | 00010 | 00000 | 01 → 0x6101).
    DECOMP_ILLEGAL(0x6101);
})

TESTCASE(decomp_q1_lui, {
    // C.LUI: rd=x1, nzimm=1  (in[12]=0, in[6:2]=00001)
    // bits: 011 | 0 | 00001 | 00001 | 01 → 0x6085
    DECOMP_OK(0x6085, rv_enc_u(1, 1, 0x37));

    // nzimm=0 is reserved (bits: 011 | 0 | 00001 | 00000 | 01 → 0x6081).
    DECOMP_ILLEGAL(0x6081);
})

TESTCASE(decomp_q1_misc_alu, {
    // C.SRLI: rd'=x8(000), shamt=1  → SRLI x8, x8, 1
    // bits: 100|0|00|000|00001|01 = 1000_0000_0000_0101 = 0x8005
    DECOMP_OK(0x8005, rv_enc_i(1, 8, 5, 8, 0x13));

    // C.SRAI: rd'=x8, shamt=1  → SRAI x8, x8, 1  (funct2=01 → bit10=1)
    // bits: 100|0|01|000|00001|01 = 1000_0100_0000_0101 = 0x8405
    DECOMP_OK(0x8405, rv_enc_i((1 << 10) | 1, 8, 5, 8, 0x13));

    // C.ANDI: rd'=x8, imm=-1  (funct2=10 → bit11=1, bit10=0)
    // bits: 100|1|10|000|11111|01 = 1001_1000_0111_1101 = 0x987D
    DECOMP_OK(0x987D, rv_enc_i(-1, 8, 7, 8, 0x13));

    // C.SUB: rd'=x8, rs2'=x9(001)  → SUB x8, x8, x9
    // bits: 100 | 0 | 11 | 000 | 00 | 001 | 01 → 0x8C05
    DECOMP_OK(0x8C05, rv_enc_r(0x20, 9, 8, 0, 8, 0x33));

    // C.XOR: rd'=x8, rs2'=x9  → XOR x8, x8, x9
    // bits: 100 | 0 | 11 | 000 | 01 | 001 | 01 → 0x8C25
    DECOMP_OK(0x8C25, rv_enc_r(0x00, 9, 8, 4, 8, 0x33));

    // C.ADDW: rd'=x8, rs2'=x9  (bit12=1 → W variant, op=01 → ADDW)
    // bits: 100 | 1 | 11 | 000 | 01 | 001 | 01 → 0x9C25
    DECOMP_OK(0x9C25, rv_enc_r(0x00, 9, 8, 0, 8, 0x3B));
})

TESTCASE(decomp_q1_j, {
    // C.J: offset=2  (offset[1]=1 → in[3]=1)
    // bits: 101 | 0|0|00|0|0|0|010|0 | 01 → 0xA009
    DECOMP_OK(0xA009, rv_enc_j(2, 0, 0x6F));
})

TESTCASE(decomp_q1_beqz, {
    // C.BEQZ: rs1'=x8(000), offset=2  (offset[1]=1 → in[3]=1)
    // bits: 110 | 0 | 00 | 000 | 00|01|0 | 01 → 0xC009
    DECOMP_OK(0xC009, rv_enc_b(2, 0, 8, 0, 0x63));
})

TESTCASE(decomp_q1_bnez, {
    // C.BNEZ: rs1'=x8, offset=2
    // bits: 111 | 0 | 00 | 000 | 00|01|0 | 01 → 0xE009
    DECOMP_OK(0xE009, rv_enc_b(2, 0, 8, 1, 0x63));
})

// ── Quadrant 2 ───────────────────────────────────────────────────────────────

TESTCASE(decomp_q2_slli, {
    // C.SLLI: rd=x2, shamt=1
    // bits: 000 | 0 | 00010 | 00001 | 10 → 0x0106
    DECOMP_OK(0x0106, rv_enc_i(1, 2, 1, 2, 0x13));
})

TESTCASE(decomp_q2_lwsp, {
    // C.LWSP: rd=x1, offset=4  (uimm[2]=1 → in[4]=1)
    // uimm[4:2]=in[6:4], uimm[7:6]=in[3:2], uimm[5]=in[12]
    // bits: 010 | 0 | 00001 | 0|01|0|00 | 10 → 0x4092
    DECOMP_OK(0x4092, rv_enc_i(4, 2, 2, 1, 0x03));
})

TESTCASE(decomp_q2_ldsp, {
    // C.LDSP: rd=x1, offset=8  (uimm[3]=1 → in[5]=1)
    // uimm[4:3]=in[6:5], uimm[8:6]=in[4:2], uimm[5]=in[12]
    // bits: 011 | 0 | 00001 | 01 | 000 | 10 → 0x60A2
    DECOMP_OK(0x60A2, rv_enc_i(8, 2, 3, 1, 0x03));
})

TESTCASE(decomp_q2_jr, {
    // C.JR: rs1=x1  → JALR x0, 0(x1)
    // bits: 100 | 0 | 00001 | 00000 | 10 → 0x8082
    DECOMP_OK(0x8082, rv_enc_i(0, 1, 0, 0, 0x67));
})

TESTCASE(decomp_q2_mv, {
    // C.MV: rd=x3, rs2=x7  → ADD x3, x0, x7
    // bits: 100 | 0 | 00011 | 00111 | 10 → 0x819E
    DECOMP_OK(0x819E, rv_enc_r(0, 7, 0, 0, 3, 0x33));
})

TESTCASE(decomp_q2_jalr, {
    // C.JALR: rs1=x5  → JALR x1, 0(x5)
    // bits: 100 | 1 | 00101 | 00000 | 10 → 0x9282
    DECOMP_OK(0x9282, rv_enc_i(0, 5, 0, 1, 0x67));
})

TESTCASE(decomp_q2_add, {
    // C.ADD: rd=x1, rs2=x2  → ADD x1, x1, x2
    // bits: 100 | 1 | 00001 | 00010 | 10 → 0x908A
    DECOMP_OK(0x908A, rv_enc_r(0, 2, 1, 0, 1, 0x33));
})

TESTCASE(decomp_q2_ebreak, {
    // C.EBREAK: in[12]=1, rs1=0, rs2=0
    // bits: 100 | 1 | 00000 | 00000 | 10 → 0x9002
    // Expands to EBREAK: SYSTEM opcode, imm=1
    DECOMP_OK(0x9002, (1u << 20) | 0x73u);
})

TESTCASE(decomp_q2_swsp, {
    // C.SWSP: rs2=x1, offset=4  (uimm[2]=1 → in[9]=1)
    // uimm[5:2]=in[12:9], uimm[7:6]=in[8:7]
    // bits: 110 | 0|0|0|1 | 00 | 00001 | 10 → 0xC206
    DECOMP_OK(0xC206, rv_enc_s(4, 1, 2, 2, 0x23));
})

TESTCASE(decomp_q2_sdsp, {
    // C.SDSP: rs2=x1, offset=8  (uimm[3]=1 → in[10]=1)
    // uimm[5:3]=in[12:10], uimm[8:6]=in[9:7]
    // bits: 111 | 0|0|1 | 000 | 00001 | 10 → 0xE406
    DECOMP_OK(0xE406, rv_enc_s(8, 1, 2, 3, 0x23));
})
