
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "softfloat.h"

#include <stdint.h>

#ifdef __x86_64__
#include <immintrin.h>
#endif

// How many bits wide a vector register is at most.
// Actual width may depend on availability of SSE, AVX and AVX2.
#define RV_MAX_VLEN 512

// One vector register.
union [[gnu::aligned(64)]] rv_vreg {
#ifdef __x86_64__
    // x86 acceleration types.
    __m128 sse;
    __m256 avx;
    __m512 avx512;
#endif
    // Various integer types.
    uint8_t   u_8[RV_MAX_VLEN / 8];
    uint16_t  u_16[RV_MAX_VLEN / 16];
    uint32_t  u_32[RV_MAX_VLEN / 32];
    uint64_t  u_64[RV_MAX_VLEN / 64];
    int8_t    i_8[RV_MAX_VLEN / 8];
    int16_t   i_16[RV_MAX_VLEN / 16];
    int32_t   i_32[RV_MAX_VLEN / 32];
    int64_t   i_64[RV_MAX_VLEN / 64];
    // Various float types.
    float     f_32[RV_MAX_VLEN / 32];
    double    f_64[RV_MAX_VLEN / 64];
    float32_t sf_32[RV_MAX_VLEN / 32];
    float64_t sf_64[RV_MAX_VLEN / 64];
};

#ifdef __x86_64__
extern bool rv_accel_sse;
extern bool rv_accel_avx;
extern bool rv_accel_avx2;
extern bool rv_accel_avx512;
#endif

// Detect available acceleration extensions.
void rv_detect_vector_accel();
