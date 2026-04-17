
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "testcase.h"

#include <limits.h>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Winteger-overflow"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Woverflow"
#endif

#define OP_TEST_CASE(insn, data_type, oper, lhs, rhs)                                                                  \
    {                                                                                                                  \
        cpu->xregs[1] = (data_type)(lhs);                                                                              \
        cpu->xregs[2] = (data_type)(rhs);                                                                              \
        rv_forcefeed_insn(machine, cpu, (insn));                                                                       \
        TEST_REG(3, (data_type)(lhs)oper(data_type)(rhs));                                                             \
    }

#define OP_TEST(name, insn, data_type, oper)                                                                           \
    TESTCASE(op_##name, {                                                                                              \
        OP_TEST_CASE(insn, data_type, oper, 0, 0);                                                                     \
        OP_TEST_CASE(insn, data_type, oper, 0, 1);                                                                     \
        OP_TEST_CASE(insn, data_type, oper, 1, 0);                                                                     \
        OP_TEST_CASE(insn, data_type, oper, 0, -1);                                                                    \
        OP_TEST_CASE(insn, data_type, oper, -1, 0);                                                                    \
        OP_TEST_CASE(insn, data_type, oper, 25683, 63);                                                                \
        OP_TEST_CASE(insn, data_type, oper, 6444295532, 32);                                                           \
        OP_TEST_CASE(insn, data_type, oper, -2542, 15);                                                                \
        OP_TEST_CASE(insn, data_type, oper, -191210000, 2);                                                            \
        OP_TEST_CASE(insn, data_type, oper, INT_MAX, INT_MAX);                                                         \
        OP_TEST_CASE(insn, data_type, oper, INT_MAX, INT_MIN);                                                         \
        OP_TEST_CASE(insn, data_type, oper, INT_MIN, INT_MIN);                                                         \
        OP_TEST_CASE(insn, data_type, oper, (int)0xcafebabe, 3);                                                       \
        OP_TEST_CASE(insn, data_type, oper, (int)0xcafebabe, (int)0xf00dbabe);                                         \
    })

OP_TEST(add, 0x002081b3, int64_t, +)
OP_TEST(sub, 0x402081b3, int64_t, -)
OP_TEST(slt, 0x0020a1b3, int64_t, <)
OP_TEST(sltu, 0x0020b1b3, uint64_t, <)
OP_TEST(or, 0x0020e1b3, int64_t, |)
OP_TEST(and, 0x0020f1b3, int64_t, &)
OP_TEST(xor, 0x0020c1b3, int64_t, ^)

OP_TEST(addw, 0x002081bb, int32_t, +)
OP_TEST(subw, 0x402081bb, int32_t, -)
