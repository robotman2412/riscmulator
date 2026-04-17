
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_machine.h"
#include "testcase.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool         do_fork    = true;
static size_t       test_count = 0;
static size_t       test_cap   = 0;
static char const **test_names = nullptr;
static testcase_t  *test_funcs = nullptr;

void testcase_register(char const *name, testcase_t test) {
    if (!test_cap) {
        test_names = malloc(sizeof(char const *));
        test_funcs = malloc(sizeof(testcase_t));
        test_cap   = 1;
    } else if (test_count + 1 >= test_cap) {
        test_cap   *= 2;
        test_names  = realloc(test_names, sizeof(char const *) * test_cap);
        test_funcs  = realloc(test_funcs, sizeof(testcase_t) * test_cap);
    }
    if (!test_names || !test_funcs) {
        printf("\033[31mOut of memory\033[0m\n");
        exit(1);
    }
    test_names[test_count] = name;
    test_funcs[test_count] = test;
    test_count++;
}

void testcase_error_message(char const *fmt, ...) {
    printf("\033[31m FAILED\n    ");
    va_list l;
    va_start(l, fmt);
    vprintf(fmt, l);
    va_end(l);
    printf("\033[0m\n");
}

static bool do_test_impl(char const *name, testcase_t test) {
    printf("Test %s...", name);
    fflush(stdout);
    struct rv_machine machine = {0};
    struct rv_cpu     cpu     = {0};
    bool              res     = test(&machine, &cpu);
    if (res) {
        printf("\033[32m OK\033[0m\n");
    }
    return res;
}

static bool do_test(char const *name, testcase_t test) {
    if (!do_fork) {
        return do_test_impl(name, test);
    }

    pid_t pid = fork();
    if (pid == 0) {
        exit(!do_test_impl(name, test));
    } else {
        while (1) {
            int wstatus;
            waitpid(pid, &wstatus, 0);
            if (WIFSIGNALED(wstatus)) {
                printf("\033[31m %s\033[0m\n", strsignal(WTERMSIG(wstatus)));
                return false;
            } else if (WIFEXITED(wstatus)) {
                return WEXITSTATUS(wstatus) == 0;
            }
        }
    }
}

int main(int argc, char **argv) {
    size_t total;
    size_t succ = 0;
    if (argc <= 1) {
        // Run all tests.
        total = test_count;
        for (size_t i = 0; i < test_count; i++) {
            succ += do_test(test_names[i], test_funcs[i]);
        }
    } else {
        // Run only specified tests.
        total = 0;
        for (int x = 1; x < argc; x++) {
            bool found = false;
            for (size_t i = 0; i < test_count; i++) {
                if (!strcmp(test_names[i], argv[x])) {
                    total++;
                    succ  += do_test(test_names[i], test_funcs[i]);
                    found  = true;
                    break;
                }
            }
            if (!found) {
                printf("\033[33mNo test found for '%s'\033[0m\n", argv[x]);
            }
        }
    }

    if (total == 0) {
        printf("No tests to run.\n");
    } else {
        printf("%zu/%zu (%zu%%) tests passed\n", succ, total, total * 100 / succ);
    }

    return succ < total;
}
