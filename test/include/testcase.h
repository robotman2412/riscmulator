
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_cpu.h"
#include "rv_machine.h"

#include <inttypes.h>

typedef bool (*testcase_t)(struct rv_machine *machine, struct rv_cpu *cpu);

void testcase_register(char const *name, testcase_t test);
[[gnu::format(printf, 1, 2)]]
void testcase_error_message(char const *fmt, ...);

#define TESTCASE(name, ...)                                                                                            \
    bool test_##name(struct rv_machine *machine, struct rv_cpu *cpu) {                                                 \
        {                                                                                                              \
            __VA_ARGS__                                                                                                \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
    [[gnu::constructor]] void _register_##name() {                                                                     \
        testcase_register(#name, test_##name);                                                                         \
    }

#define TEST_ASSERT(condition)                                                                                         \
    {                                                                                                                  \
        if (!(condition)) {                                                                                            \
            testcase_error_message("Test assertion `" #condition "` failed");                                          \
            return false;                                                                                              \
        }                                                                                                              \
    }

#define TEST_REG(regno, value)                                                                                         \
    {                                                                                                                  \
        int     regno_ = (regno);                                                                                      \
        int64_t value_ = (value);                                                                                      \
        if ((int64_t)cpu->xregs[regno_] != value_) {                                                                   \
            testcase_error_message(                                                                                    \
                "Expected x%d = " #value "; 0x%" PRIx64 " (%" PRId64 "), actual = 0x%" PRIx64 " (%" PRId64 ")",        \
                regno_,                                                                                                \
                value_,                                                                                                \
                value_,                                                                                                \
                (int64_t)cpu->xregs[regno_],                                                                           \
                (int64_t)cpu->xregs[regno_]                                                                            \
            );                                                                                                         \
            return false;                                                                                              \
        }                                                                                                              \
    }

#define TEST_PC(value)                                                                                                 \
    {                                                                                                                  \
        int64_t value_ = (value);                                                                                      \
        if ((int64_t)cpu->pc != (value)) {                                                                             \
            testcase_error_message(                                                                                    \
                "Expected pc = " #value ";  0x%" PRIx64 " (%" PRId64 "), actual = 0x%" PRIx64 " (%" PRId64 ")",        \
                value_,                                                                                                \
                value_,                                                                                                \
                (int64_t)cpu->pc,                                                                                      \
                (int64_t)cpu->pc                                                                                       \
            );                                                                                                         \
            return false;                                                                                              \
        }                                                                                                              \
    }
