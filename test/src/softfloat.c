
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_softfloat.h"
#include "testcase.h"

// 2.5f in IEEE 754 single precision: sign=0, exp=128 (2^1), mantissa=1.01b.
#define F32_2_5 ((float32_t){ .v = 0x40200000u })

// RNE rounds 2.5 to 2 (nearest even); RMM rounds 2.5 to 3 (away from zero).
TESTCASE_NOMACHINE(softfloat_rmm_tiebreak, {
    TEST_ASSERT(f32_to_i32(F32_2_5, softfloat_round_near_even, false) == 2);
    TEST_ASSERT(f32_to_i32(F32_2_5, softfloat_round_near_maxMag, false) == 3);
})

TESTCASE_NOMACHINE(softfloat_frm_mapping, {
    TEST_ASSERT(rv_frm_to_softfloat(RV_FRM_RNE) == softfloat_round_near_even);
    TEST_ASSERT(rv_frm_to_softfloat(RV_FRM_RTZ) == softfloat_round_minMag);
    TEST_ASSERT(rv_frm_to_softfloat(RV_FRM_RDN) == softfloat_round_min);
    TEST_ASSERT(rv_frm_to_softfloat(RV_FRM_RUP) == softfloat_round_max);
    TEST_ASSERT(rv_frm_to_softfloat(RV_FRM_RMM) == softfloat_round_near_maxMag);
})

// 1/3 is not exactly representable; the inexact flag must be raised.
TESTCASE_NOMACHINE(softfloat_flag_nx, {
    rv_softfloat_clearflags();
    softfloat_roundingMode = softfloat_round_near_even;
    f32_div(i32_to_f32(1), i32_to_f32(3));
    TEST_ASSERT(rv_softfloat_getflags() == (1 << RV_FFLAGS_NX_BIT));
})

// 0/0 is an invalid operation; the NV flag must be raised.
TESTCASE_NOMACHINE(softfloat_flag_nv, {
    rv_softfloat_clearflags();
    f32_div(i32_to_f32(0), i32_to_f32(0));
    TEST_ASSERT(rv_softfloat_getflags() & (1 << RV_FFLAGS_NV_BIT));
})

// 1/0 is division by zero; the DZ flag must be raised.
TESTCASE_NOMACHINE(softfloat_flag_dz, {
    rv_softfloat_clearflags();
    f32_div(i32_to_f32(1), i32_to_f32(0));
    TEST_ASSERT(rv_softfloat_getflags() & (1 << RV_FFLAGS_DZ_BIT));
})
