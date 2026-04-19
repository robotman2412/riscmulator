
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_impl/float.h"

#include "rv_cpu.h"
#include "rv_csr.h"
#include "rv_insn.h"
#include "rv_machine.h"
#include "rv_privileged.h"
#include "rv_softfloat.h"
#include "softfloat.h"

#include <stdint.h>

#include <fenv.h>
#include <tgmath.h>

// Float instruction funct5 values.
enum funct5 {
    FUNCT5_ADD        = 0b00000,
    FUNCT5_SUB        = 0b00001,
    FUNCT5_MUL        = 0b00010,
    FUNCT5_DIV        = 0b00011,
    FUNCT5_FCVT_FTOF  = 0b01000,
    FUNCT5_SQRT       = 0b01011,
    FUNCT5_SGNJ       = 0b00100,
    FUNCT5_MIN_MAX    = 0b00101,
    FUNCT5_FCVT_XTOF  = 0b11010,
    FUNCT5_FCVT_FTOX  = 0b11000,
    FUNCT5_MVXF_CLASS = 0b11100,
    FUNCT5_MVFX       = 0b11110,
    FUNCT5_EQ_LE_LT   = 0b10100,
};

// TODO: On RISC-V hosts with the float extensions, we could theoretically
// always do exact floating-point math.
bool rv_float_exact_math = false;

// Apply the rounding mode.
static void apply_frm(struct rv_cpu *cpu, uint32_t insn, bool *is_inexact) {
    uint32_t frm = RV_INSN_FUNCT3(insn);
    if (frm == RV_FRM_DYN) {
        frm = (cpu->csr.fcsr >> RV_FCSR_FRM_BASE_BIT) & RV_FRM_MASK;
    }

    switch (frm) {
#ifdef FE_TONEAREST
        case RV_FRM_RNE: fesetround(FE_TONEAREST); break;
#endif
#ifdef FE_TOWARDZERO
        case RV_FRM_RTZ: fesetround(FE_TOWARDZERO); break;
#endif
#ifdef FE_DOWNWARD
        case RV_FRM_RDN: fesetround(FE_DOWNWARD); break;
#endif
#ifdef FE_UPWARD
        case RV_FRM_RUP: fesetround(FE_UPWARD); break;
#endif
#ifdef FE_TONEAREST
        case RV_FRM_RMM:
            fesetround(FE_TONEAREST);
            // No stdc equivalent for this mode will produce inexact results.
            if (is_inexact) {
                *is_inexact = true;
            }
            break;
#endif

        default:
            // Invalid rounding mode will produce inexact results.
            if (is_inexact) {
                *is_inexact = true;
            }
    }
}

// Get float exception flags and set them in the `fflags` CSR.
static void check_fflags(struct rv_cpu *cpu) {
    fexcept_t flag;
    fegetexceptflag(
        &flag,
        FE_INVALID | FE_INEXACT | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW
    );
    if (fetestexceptflag(&flag, FE_INVALID)) {
        cpu->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;
    }
    if (fetestexceptflag(&flag, FE_INEXACT)) {
        cpu->csr.fcsr |= 1 << RV_FFLAGS_NX_BIT;
    }
    if (fetestexceptflag(&flag, FE_DIVBYZERO)) {
        cpu->csr.fcsr |= 1 << RV_FFLAGS_DZ_BIT;
    }
    if (fetestexceptflag(&flag, FE_OVERFLOW)) {
        cpu->csr.fcsr |= 1 << RV_FFLAGS_OF_BIT;
    }
    if (fetestexceptflag(&flag, FE_UNDERFLOW)) {
        cpu->csr.fcsr |= 1 << RV_FFLAGS_UF_BIT;
    }
}

// Canonical NaN bit patterns per the RISC-V spec.
#define RV_CANONICAL_NAN_F32 UINT32_C(0x7FC00000)
#define RV_CANONICAL_NAN_F64 UINT64_C(0x7FF8000000000000)

// Canonicalize host NaN to RISC-V canonical NaN (spec §11.3).
#define CANON_NAN_F32(res)                                                     \
    if (isnan((res).f_32)) {                                                   \
        (res).i_32 = RV_CANONICAL_NAN_F32;                                     \
    }
#define CANON_NAN_F64(res)                                                     \
    if (isnan((res).f_64)) {                                                   \
        (res).i_64 = RV_CANONICAL_NAN_F64;                                     \
    }

// Read an F32 register with NaN-boxing check.  Returns canonical NaN when the
// upper 32 bits are not all ones (i.e. the register does not hold a valid F32).
static inline union rv_freg rv_freg_read_f32(struct rv_cpu *cpu, uint32_t idx) {
    union rv_freg r = rv_freg_read(cpu, idx);
    if ((r.i_64 >> 32) != 0xFFFFFFFF) {
        return (union rv_freg){.i_64 =
                                   0xFFFFFFFF00000000 | RV_CANONICAL_NAN_F32};
    }
    return r;
}

// Returns true if the given float register value is a signaling NaN of the
// appropriate width.  Uses bit-pattern checks; issignaling() has platform
// quirks.
static inline bool rv_is_snan(union rv_freg r, bool f64) {
    if (f64) {
        // F64: exponent=0x7FF, quiet bit (bit 51) clear, payload non-zero.
        return (r.i_64 & UINT64_C(0x7FF8000000000000)) ==
                   UINT64_C(0x7FF0000000000000) &&
               (r.i_64 & UINT64_C(0x0007FFFFFFFFFFFF)) != 0;
    } else {
        // F32: exponent=0xFF, quiet bit (bit 22) clear, payload non-zero.
        return (r.i_32 & 0x7FC00000U) == 0x7F800000U &&
               (r.i_32 & 0x003FFFFFU) != 0;
    }
}

// Convert float `value` to integer `res` with RISC-V saturation semantics.
// `lo`/`hi` are the inclusive-lower / exclusive-upper bounds as double.
// Sets NV on overflow/NaN; FE_INEXACT from rint is cleared on the overflow path
// so check_fflags() only sees NX on in-range inexact conversions.
#define FTOX_HELPER(cpu, res, value, minv, maxv, lo, hi)                       \
    {                                                                          \
        if (isnan(value)) {                                                    \
            (res)            = (maxv);                                         \
            (cpu)->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;                          \
        } else {                                                               \
            double _rv = rint((double)(value));                                \
            if (!(_rv < (hi)) || !(_rv >= (lo))) {                             \
                (res)            = _rv < 0.0 ? (minv) : (maxv);                \
                (cpu)->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;                      \
                feclearexcept(FE_INEXACT);                                     \
            } else {                                                           \
                (res) = _rv;                                                   \
            }                                                                  \
        }                                                                      \
    }

static inline void
    rv_float_add(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    union rv_freg res = {.i_64 = UINT64_MAX};
    union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
    union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
    if (softfloat && f64) {
        res.sf_64 = f64_add(lhs.sf_64, rhs.sf_64);
    } else if (softfloat) {
        res.sf_32 = f32_add(lhs.sf_32, rhs.sf_32);
    } else if (f64) {
        res.f_64 = lhs.f_64 + rhs.f_64;
        CANON_NAN_F64(res);
    } else {
        res.f_32 = lhs.f_32 + rhs.f_32;
        CANON_NAN_F32(res);
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_sub(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    union rv_freg res = {.i_64 = UINT64_MAX};
    union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
    union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
    if (softfloat && f64) {
        res.sf_64 = f64_sub(lhs.sf_64, rhs.sf_64);
    } else if (softfloat) {
        res.sf_32 = f32_sub(lhs.sf_32, rhs.sf_32);
    } else if (f64) {
        res.f_64 = lhs.f_64 - rhs.f_64;
        CANON_NAN_F64(res);
    } else {
        res.f_32 = lhs.f_32 - rhs.f_32;
        CANON_NAN_F32(res);
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_mul(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    union rv_freg res = {.i_64 = UINT64_MAX};
    union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
    union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
    if (softfloat && f64) {
        res.sf_64 = f64_mul(lhs.sf_64, rhs.sf_64);
    } else if (softfloat) {
        res.sf_32 = f32_mul(lhs.sf_32, rhs.sf_32);
    } else if (f64) {
        res.f_64 = lhs.f_64 * rhs.f_64;
        CANON_NAN_F64(res);
    } else {
        res.f_32 = lhs.f_32 * rhs.f_32;
        CANON_NAN_F32(res);
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_div(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    union rv_freg res = {.i_64 = UINT64_MAX};
    union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
    union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
    if (softfloat && f64) {
        res.sf_64 = f64_div(lhs.sf_64, rhs.sf_64);
    } else if (softfloat) {
        res.sf_32 = f32_div(lhs.sf_32, rhs.sf_32);
    } else if (f64) {
        res.f_64 = lhs.f_64 / rhs.f_64;
        CANON_NAN_F64(res);
    } else {
        res.f_32 = lhs.f_32 / rhs.f_32;
        CANON_NAN_F32(res);
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_sqrt(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    union rv_freg res = {.i_64 = UINT64_MAX};
    union rv_freg arg = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));

    if (f64) {
        if (softfloat) {
            res.sf_64 = f64_sqrt(arg.sf_64);
        } else {
            res.f_64 = sqrt(arg.f_64);
        }
        CANON_NAN_F64(res);
    } else {
        if (softfloat) {
            res.sf_32 = f32_sqrt(arg.sf_32);
        } else {
            res.f_32 = sqrt(arg.f_32);
        }
        CANON_NAN_F32(res);
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_ftof(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    bool src_f64 = RV_INSN_RS2(insn) == 1;
    // TODO: It should be impossible for src_f64 == f64, and for rs2 > 1.
    // However, no machine parameter -> can't trap.

    union rv_freg arg = src_f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));

    union rv_freg res = {.i_64 = UINT64_MAX};
    if (f64) {
        // Convert f32->f64; never rounds.
        res.f_64 = arg.f_32;
        CANON_NAN_F64(res);
    } else {
        // Convert f64->f32.
        if (softfloat) {
            res.sf_32 = f64_to_f32(arg.sf_64);
        } else {
            res.f_32 = arg.f_64;
        }
        CANON_NAN_F32(res);
    }

    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_xtof(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    bool          is_unsigned = RV_INSN_RS2(insn) & 1;
    bool          x64         = RV_INSN_RS2(insn) & 2;
    int64_t       arg         = rv_xreg_read(cpu, RV_INSN_RS1(insn));
    union rv_freg res         = {.i_64 = UINT64_MAX};
    if (is_unsigned && x64) {
        // float to u64
        if (softfloat && f64) {
            res.sf_64 = ui64_to_f64(arg);
        } else if (softfloat) {
            res.sf_32 = ui64_to_f32(arg);
        } else if (f64) {
            res.f_64 = (uint64_t)arg;
        } else {
            res.f_32 = (uint64_t)arg;
        }
    } else if (x64) {
        // float to i64
        if (softfloat && f64) {
            res.sf_64 = i64_to_f64(arg);
        } else if (softfloat) {
            res.sf_32 = i64_to_f32(arg);
        } else if (f64) {
            res.f_64 = arg;
        } else {
            res.f_32 = arg;
        }
    } else if (is_unsigned) {
        // float to u32
        if (softfloat && f64) {
            res.sf_64 = ui32_to_f64(arg);
        } else if (softfloat) {
            res.sf_32 = ui32_to_f32(arg);
        } else if (f64) {
            res.f_64 = (uint32_t)arg;
        } else {
            res.f_32 = (uint32_t)arg;
        }
    } else {
        // float to i32
        if (softfloat && f64) {
            res.sf_64 = i32_to_f64(arg);
        } else if (softfloat) {
            res.sf_32 = i32_to_f32(arg);
        } else if (f64) {
            res.f_64 = (int32_t)arg;
        } else {
            res.f_32 = (int32_t)arg;
        }
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_ftox(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    bool          is_unsigned = RV_INSN_RS2(insn) & 1;
    bool          x64         = RV_INSN_RS2(insn) & 2;
    union rv_freg arg         = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                                    : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
    int64_t       res;
    if (is_unsigned && x64) {
        if (softfloat && f64) {
            res = f64_to_ui64(arg.sf_64, softfloat_roundingMode, true);
        } else if (softfloat) {
            res = f32_to_ui64(arg.sf_32, softfloat_roundingMode, true);
        } else if (f64) {
            uint64_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_64,
                (uint64_t)0,
                UINT64_MAX,
                0.0,
                18446744073709551616.0
            );
            res = tmp;
        } else {
            uint64_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_32,
                (uint64_t)0,
                UINT64_MAX,
                0.0,
                18446744073709551616.0
            );
            res = tmp;
        }
    } else if (x64) {
        if (softfloat && f64) {
            res = f64_to_i64(arg.sf_64, softfloat_roundingMode, true);
        } else if (softfloat) {
            res = f32_to_i64(arg.sf_32, softfloat_roundingMode, true);
        } else if (f64) {
            int64_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_64,
                INT64_MIN,
                INT64_MAX,
                -9223372036854775808.0,
                9223372036854775808.0
            );
            res = tmp;
        } else {
            int64_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_32,
                INT64_MIN,
                INT64_MAX,
                -9223372036854775808.0,
                9223372036854775808.0
            );
            res = tmp;
        }
    } else if (is_unsigned) {
        if (softfloat && f64) {
            res = f64_to_ui32(arg.sf_64, softfloat_roundingMode, true);
        } else if (softfloat) {
            res = f32_to_ui32(arg.sf_32, softfloat_roundingMode, true);
        } else if (f64) {
            uint32_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_64,
                (uint32_t)0,
                UINT32_MAX,
                0.0,
                4294967296.0
            );
            res = tmp;
        } else {
            uint32_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_32,
                (uint32_t)0,
                UINT32_MAX,
                0.0,
                4294967296.0
            );
            res = tmp;
        }
        // RISC-V sign-extends unsigned 32-bit values too.
        res = (int32_t)res;
    } else {
        if (softfloat && f64) {
            res = f64_to_i32(arg.sf_64, softfloat_roundingMode, true);
        } else if (softfloat) {
            res = f32_to_i32(arg.sf_32, softfloat_roundingMode, true);
        } else if (f64) {
            int32_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_64,
                INT32_MIN,
                INT32_MAX,
                -2147483648.0,
                2147483648.0
            );
            res = tmp;
        } else {
            int32_t tmp;
            FTOX_HELPER(
                cpu,
                tmp,
                arg.f_32,
                INT32_MIN,
                INT32_MAX,
                -2147483648.0,
                2147483648.0
            );
            res = tmp;
        }
    }
    rv_xreg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void rv_float_mvxf_fclass(
    struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64
) {
    (void)softfloat;
    // fmv.x.w/d (funct3=0) transfers raw bits; fclass (funct3=1) interprets
    // the float value and must respect NaN-boxing.
    union rv_freg arg = (RV_INSN_FUNCT3(insn) == 1 && !f64)
                            ? rv_freg_read_f32(cpu, RV_INSN_RS1(insn))
                            : rv_freg_read(cpu, RV_INSN_RS1(insn));

    int64_t res;
    if (RV_INSN_FUNCT3(insn) == 1) {
        res = 0;

        // Except from the RISC-V specification:
        // Bit Meaning
        // 0   rs1 is -inf
        // 1   rs1 is a negative normal number.
        // 2   rs1 is a negative subnormal number.
        // 3   rs1 is -0
        // 4   rs1 is +0
        // 5   rs1 is a positive subnormal number.
        // 6   rs1 is a positive normal number.
        // 7   rs1 is +inf
        // 8   rs1 is a signaling NaN.
        // 9   rs1 is a quiet NaN.

        if (f64) {
            if (isinf(arg.f_64)) {
                if (signbit(arg.f_64)) {
                    res |= 1 << 0;
                } else {
                    res |= 1 << 7;
                }
            }
            if (iszero(arg.f_64)) {
                if (signbit(arg.f_64)) {
                    res |= 1 << 3;
                } else {
                    res |= 1 << 4;
                }
            }
            if (isnormal(arg.f_64)) {
                if (signbit(arg.f_64)) {
                    res |= 1 << 1;
                } else {
                    res |= 1 << 6;
                }
            }
            if (issubnormal(arg.f_64)) {
                if (signbit(arg.f_64)) {
                    res |= 1 << 2;
                } else {
                    res |= 1 << 5;
                }
            }
            if (rv_is_snan(arg, true)) {
                res |= 1 << 8;
            } else if (isnan(arg.f_64)) {
                res |= 1 << 9;
            }
        } else {
            if (isinf(arg.f_32)) {
                if (signbit(arg.f_32)) {
                    res |= 1 << 0;
                } else {
                    res |= 1 << 7;
                }
            }
            if (iszero(arg.f_32)) {
                if (signbit(arg.f_32)) {
                    res |= 1 << 3;
                } else {
                    res |= 1 << 4;
                }
            }
            if (isnormal(arg.f_32)) {
                if (signbit(arg.f_32)) {
                    res |= 1 << 1;
                } else {
                    res |= 1 << 6;
                }
            }
            if (issubnormal(arg.f_32)) {
                if (signbit(arg.f_32)) {
                    res |= 1 << 2;
                } else {
                    res |= 1 << 5;
                }
            }
            if (rv_is_snan(arg, false)) {
                res |= 1 << 8;
            } else if (isnan(arg.f_32)) {
                res |= 1 << 9;
            }
        }

    } else {
        if (f64) {
            res = arg.i_64;
        } else {
            res = (int32_t)arg.i_32;
        }
    }

    rv_xreg_write(cpu, RV_INSN_RD(insn), res);
}

static inline void
    rv_float_mvfx(struct rv_cpu *cpu, uint32_t insn, bool softfloat, bool f64) {
    (void)softfloat;
    int64_t arg = rv_xreg_read(cpu, RV_INSN_RS1(insn));

    union rv_freg res = {.i_64 = UINT64_MAX};
    if (f64) {
        res.i_64 = arg;
    } else {
        res.i_32 = arg;
    }

    rv_freg_write(cpu, RV_INSN_RD(insn), res);
}

// Execute an instruction under the OP-FP major opcode.
void rv_float_op_fp(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    feclearexcept(
        FE_INVALID | FE_INEXACT | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW
    );
    rv_softfloat_clearflags();

    if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
        // Float ops disabled.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    // Check operation size.
    enum rv_ffmt ffmt = RV_INSN_FFMT(insn);
    bool         f64;
    switch (ffmt) {
        case RV_FFMT_F32: f64 = false; break;
        case RV_FFMT_F64: f64 = true; break;
        default:
            // Not supported.
            rv_do_iillegal(machine, cpu, insn);
            return;
    }

    // Operations without rounding mode.
    enum funct5 funct5 = RV_INSN_FUNCT5(insn);
    if (funct5 == FUNCT5_EQ_LE_LT) {
        union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
        union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
        bool          res;

        switch (RV_INSN_FUNCT3(insn)) {
            case 2: // feq
                res = f64 ? lhs.f_64 == rhs.f_64 : lhs.f_32 == rhs.f_32;
                break;
            case 1: // flt
                res = f64 ? lhs.f_64 < rhs.f_64 : lhs.f_32 < rhs.f_32;
                break;
            case 0: // fle
                res = f64 ? lhs.f_64 <= rhs.f_64 : lhs.f_32 <= rhs.f_32;
                break;
            default: rv_do_iillegal(machine, cpu, insn); return;
        }

        rv_xreg_write(cpu, RV_INSN_RD(insn), res);
        check_fflags(cpu);
        return;

    } else if (funct5 == FUNCT5_SGNJ) {
        // Note: Unlike arithmetic, these do not set flags nor canonicalize
        // NaNs. NaN-boxing still applies when reading the rs1 value as F32.
        union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
        union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
        union rv_freg res;
        uint64_t      sign_bit;
        if (f64) {
            res      = lhs;
            sign_bit = (uint64_t)1 << 63;
        } else {
            res.i_64 = UINT64_MAX;
            res.i_32 = lhs.i_32;
            sign_bit = (uint64_t)1 << 31;
        }

        switch (RV_INSN_FUNCT3(insn)) {
            case 0: // fsgnj
                res.i_64 &= ~sign_bit;
                res.i_64 |= rhs.i_64 & sign_bit;
                break;
            case 1: // fsgnjn
                res.i_64 &= ~sign_bit;
                res.i_64 |= ~rhs.i_64 & sign_bit;
                break;
            case 2: // fsgnjx
                res.i_64 ^= rhs.i_64 & sign_bit;
                break;
            default: rv_do_iillegal(machine, cpu, insn); return;
        }

        rv_freg_write(cpu, RV_INSN_RD(insn), res);
        return;

    } else if (funct5 == FUNCT5_MIN_MAX) {
        union rv_freg lhs = f64 ? rv_freg_read(cpu, RV_INSN_RS1(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
        union rv_freg rhs = f64 ? rv_freg_read(cpu, RV_INSN_RS2(insn))
                                : rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
        union rv_freg res = {.i_64 = UINT64_MAX};

        if (rv_is_snan(lhs, f64) || rv_is_snan(rhs, f64)) {
            cpu->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;
        }

        // RISC-V: if one operand is NaN, return the other; if both are NaN,
        // return canonical NaN. C's fmin/fmax with sNaN operands may return
        // the quieted NaN instead of the non-NaN operand, so handle manually.
        bool lhs_nan = f64 ? isnan(lhs.f_64) : isnan(lhs.f_32);
        bool rhs_nan = f64 ? isnan(rhs.f_64) : isnan(rhs.f_32);

        if (lhs_nan && rhs_nan) {
            if (f64) {
                res.i_64 = RV_CANONICAL_NAN_F64;
            } else {
                res.i_64 = UINT64_MAX;
                res.i_32 = RV_CANONICAL_NAN_F32;
            }
        } else if (lhs_nan) {
            res = rhs;
        } else if (rhs_nan) {
            res = lhs;
        } else {
            bool is_min        = RV_INSN_FUNCT3(insn) == 0;
            // RISC-V: fmin(-0,+0)=-0, fmax(-0,+0)=+0. C's fmin/fmax doesn't
            // guarantee this ordering, so detect the both-zero case manually.
            bool both_zero_f32 = !f64 && (lhs.i_32 & 0x7FFFFFFF) == 0 &&
                                 (rhs.i_32 & 0x7FFFFFFF) == 0;
            bool both_zero_f64 =
                f64 && (lhs.i_64 & UINT64_C(0x7FFFFFFFFFFFFFFF)) == 0 &&
                (rhs.i_64 & UINT64_C(0x7FFFFFFFFFFFFFFF)) == 0;
            if (both_zero_f32) {
                // fmin returns -0, fmax returns +0
                res.i_64 = UINT64_MAX;
                res.i_32 = is_min ? 0x80000000U : 0x00000000U;
            } else if (both_zero_f64) {
                res.i_64 = is_min ? UINT64_C(0x8000000000000000) : 0;
            } else {
                switch (RV_INSN_FUNCT3(insn)) {
                    case 0: // fmin
                        if (f64) {
                            res.f_64 = fmin(lhs.f_64, rhs.f_64);
                        } else {
                            res.i_64 = UINT64_MAX;
                            res.f_32 = fmin(lhs.f_32, rhs.f_32);
                        }
                        break;
                    case 1: // fmax
                        if (f64) {
                            res.f_64 = fmax(lhs.f_64, rhs.f_64);
                        } else {
                            res.i_64 = UINT64_MAX;
                            res.f_32 = fmax(lhs.f_32, rhs.f_32);
                        }
                        break;
                    default: rv_do_iillegal(machine, cpu, insn); return;
                }
            }
        }

        if (RV_INSN_FUNCT3(insn) > 1) {
            rv_do_iillegal(machine, cpu, insn);
            return;
        }

        rv_freg_write(cpu, RV_INSN_RD(insn), res);
        return;
    }

    // The remaining operations do use the rounding mode.
    bool inexact_frm = false;
    apply_frm(cpu, insn, &inexact_frm);
    bool softfloat = rv_float_exact_math && inexact_frm;
    if (softfloat) {
        rv_softfloat_setround(RV_INSN_FUNCT3(insn));
    }

    switch (funct5) {
        case FUNCT5_ADD: rv_float_add(cpu, insn, softfloat, f64); break;
        case FUNCT5_SUB: rv_float_sub(cpu, insn, softfloat, f64); break;
        case FUNCT5_MUL: rv_float_mul(cpu, insn, softfloat, f64); break;
        case FUNCT5_DIV: rv_float_div(cpu, insn, softfloat, f64); break;
        case FUNCT5_SQRT: rv_float_sqrt(cpu, insn, softfloat, f64); break;

        case FUNCT5_FCVT_FTOF: rv_float_ftof(cpu, insn, softfloat, f64); break;
        case FUNCT5_FCVT_XTOF: rv_float_xtof(cpu, insn, softfloat, f64); break;
        case FUNCT5_FCVT_FTOX: rv_float_ftox(cpu, insn, softfloat, f64); break;

        case FUNCT5_MVXF_CLASS:
            rv_float_mvxf_fclass(cpu, insn, softfloat, f64);
            break;
        case FUNCT5_MVFX: rv_float_mvfx(cpu, insn, softfloat, f64); break;

        default: rv_do_iillegal(machine, cpu, insn); return;
    }

    if (softfloat) {
        cpu->csr.fcsr |= rv_softfloat_getflags();
    } else {
        check_fflags(cpu);
    }
}

// Execute an instruction under the MADD, MSUB, NMADD or NMSUB major opcodes.
void rv_float_fmadd(
    struct rv_machine *machine, struct rv_cpu *cpu, uint32_t insn
) {
    feclearexcept(
        FE_INVALID | FE_INEXACT | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW
    );
    rv_softfloat_clearflags();

    if (!RV_CHECK_XS(cpu->csr.mstatus, RV_STATUS_FS_BASE_BIT)) {
        // Float ops disabled.
        rv_do_iillegal(machine, cpu, insn);
        return;
    }

    // Check operation size.
    enum rv_ffmt ffmt = RV_INSN_FFMT(insn);
    bool         f64;
    switch (ffmt) {
        case RV_FFMT_F32: f64 = false; break;
        case RV_FFMT_F64: f64 = true; break;
        default:
            // Not supported.
            rv_do_iillegal(machine, cpu, insn);
            return;
    }

    bool subtract   = RV_INSN_OP_MAJ(insn) & 0b00001;
    bool negate_mul = RV_INSN_OP_MAJ(insn) & 0b00010;

    // Apply rounding mode.
    bool inexact_frm = false;
    apply_frm(cpu, insn, &inexact_frm);
    bool softfloat = rv_float_exact_math && inexact_frm;
    if (softfloat) {
        rv_softfloat_setround(RV_INSN_FUNCT3(insn));
    }

    union rv_freg res = {.i_64 = UINT64_MAX};
    if (f64) {
        union rv_freg rs1 = rv_freg_read(cpu, RV_INSN_RS1(insn));
        union rv_freg rs2 = rv_freg_read(cpu, RV_INSN_RS2(insn));
        union rv_freg rs3 = rv_freg_read(cpu, RV_INSN_RS3(insn));

        if ((isinf(rs1.f_64) && iszero(rs2.f_64)) ||
            (isinf(rs2.f_64) && iszero(rs1.f_64))) {
            cpu->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;
        }
        if (negate_mul) {
            rs1.i_64 ^= UINT64_C(1) << 63;
        }
        if (subtract) {
            rs3.i_64 ^= UINT64_C(1) << 63;
        }

        if (softfloat) {
            res.sf_64 = f64_mulAdd(rs1.sf_64, rs2.sf_64, rs3.sf_64);
        } else {
            res.f_64 = fma(rs1.f_64, rs2.f_64, rs3.f_64);
        }
    } else {
        union rv_freg rs1 = rv_freg_read_f32(cpu, RV_INSN_RS1(insn));
        union rv_freg rs2 = rv_freg_read_f32(cpu, RV_INSN_RS2(insn));
        union rv_freg rs3 = rv_freg_read_f32(cpu, RV_INSN_RS3(insn));

        if ((isinf(rs1.f_32) && iszero(rs2.f_32)) ||
            (isinf(rs2.f_32) && iszero(rs1.f_32))) {
            cpu->csr.fcsr |= 1 << RV_FFLAGS_NV_BIT;
        }
        if (negate_mul) {
            rs1.i_32 ^= UINT32_C(1) << 31;
        }
        if (subtract) {
            rs3.i_32 ^= UINT32_C(1) << 31;
        }

        if (softfloat) {
            res.sf_32 = f32_mulAdd(rs1.sf_32, rs2.sf_32, rs3.sf_32);
        } else {
            res.f_32 = fma(rs1.f_32, rs2.f_32, rs3.f_32);
        }
    }
    rv_freg_write(cpu, RV_INSN_RD(insn), res);

    if (softfloat) {
        cpu->csr.fcsr |= rv_softfloat_getflags();
    } else {
        check_fflags(cpu);
    }
}
