
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

// Decompress a 16-bit instruction into its 32-bit equivalent.
// Returns true on success; returns false for reserved/illegal encodings.
bool rv_decompress(uint16_t in, uint32_t *out);

// ── 32-bit instruction encoders ──────────────────────────────────────────────
// These are used by rv_decompress.c and its tests.

static inline uint32_t
    rv_enc_r(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static inline uint32_t rv_enc_i(int32_t imm12, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return ((uint32_t)(imm12 & 0xfff) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static inline uint32_t rv_enc_s(int32_t imm12, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    return ((uint32_t)((imm12 >> 5) & 0x7f) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) |
           ((uint32_t)(imm12 & 0x1f) << 7) | opcode;
}

static inline uint32_t rv_enc_u(int32_t imm20, uint32_t rd, uint32_t opcode) {
    return ((uint32_t)(imm20 & 0xfffff) << 12) | (rd << 7) | opcode;
}

static inline uint32_t rv_enc_b(int32_t imm13, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode) {
    return ((uint32_t)((imm13 >> 12) & 1) << 31) | ((uint32_t)((imm13 >> 5) & 0x3f) << 25) | (rs2 << 20) | (rs1 << 15) |
           (funct3 << 12) | ((uint32_t)((imm13 >> 1) & 0xf) << 8) | ((uint32_t)((imm13 >> 11) & 1) << 7) | opcode;
}

static inline uint32_t rv_enc_j(int32_t imm21, uint32_t rd, uint32_t opcode) {
    return ((uint32_t)((imm21 >> 20) & 1) << 31) | ((uint32_t)((imm21 >> 1) & 0x3ff) << 21) |
           ((uint32_t)((imm21 >> 11) & 1) << 20) | ((uint32_t)((imm21 >> 12) & 0xff) << 12) | (rd << 7) | opcode;
}
