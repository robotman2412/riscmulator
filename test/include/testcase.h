
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include "rv_cpu.h"
#include "rv_machine.h"

#include <inttypes.h>

typedef bool (*testcase_t)(void);

void testcase_register(char const *name, testcase_t test);
[[gnu::format(printf, 1, 2)]]
void testcase_error_message(char const *fmt, ...);

// TESTCASE: creates a machine (1 CPU, 0x1000 bytes RAM at 0x10000) before the
// body and destroys it after.  The body may use 'machine' and 'cpu' directly.
#define TESTCASE(name, ...)                                                                                            \
    static bool _testbody_##name(struct rv_machine *machine, struct rv_cpu *cpu) {                                     \
        {                                                                                                              \
            __VA_ARGS__                                                                                                \
        }                                                                                                              \
        return true;                                                                                                   \
    }                                                                                                                  \
    bool test_##name(void) {                                                                                           \
        struct rv_machine _machine = {0};                                                                              \
        if (!rv_machine_init(&_machine, 1, 0x10000, 0x1000)) {                                                         \
            testcase_error_message("rv_machine_init failed");                                                          \
            return false;                                                                                              \
        }                                                                                                              \
        bool _result = _testbody_##name(&_machine, &_machine.cpus[0]);                                                 \
        rv_machine_destroy(&_machine);                                                                                 \
        return _result;                                                                                                \
    }                                                                                                                  \
    [[gnu::constructor]] void _register_##name() {                                                                     \
        testcase_register(#name, test_##name);                                                                         \
    }

// TESTCASE_NOMACHINE: no machine or CPU is created.
#define TESTCASE_NOMACHINE(name, ...)                                                                                  \
    bool test_##name(void) {                                                                                           \
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
        int64_t value_ = (int64_t)(value);                                                                             \
        if ((int64_t)cpu->pc != value_) {                                                                              \
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
