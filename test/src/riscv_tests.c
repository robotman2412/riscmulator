
// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_cpu.h"
#include "rv_machine.h"
#include "rv_privileged.h"
#include "testcase.h"

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool do_riscv_test(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    char const        *set,
    char const        *test
);

#define RISCV_TEST2(set, set_str, test, test_str)                              \
    TESTCASE(                                                                  \
        riscv_##set##_##test,                                                  \
        return do_riscv_test(machine, cpu, set_str, test_str);                 \
    )
#define RISCV_TEST1(set, test) RISCV_TEST2(set, #set, test, #test)

RISCV_TEST1(rv64ui, add)
RISCV_TEST1(rv64ui, addi)
RISCV_TEST1(rv64ui, addiw)
RISCV_TEST1(rv64ui, addw)
RISCV_TEST1(rv64ui, and)
RISCV_TEST1(rv64ui, andi)
RISCV_TEST1(rv64ui, auipc)
RISCV_TEST1(rv64ui, beq)
RISCV_TEST1(rv64ui, bge)
RISCV_TEST1(rv64ui, bgeu)
RISCV_TEST1(rv64ui, blt)
RISCV_TEST1(rv64ui, bltu)
RISCV_TEST1(rv64ui, bne)
RISCV_TEST1(rv64ui, simple)
RISCV_TEST1(rv64ui, fence_i)
RISCV_TEST1(rv64ui, jal)
RISCV_TEST1(rv64ui, jalr)
RISCV_TEST1(rv64ui, lb)
RISCV_TEST1(rv64ui, lbu)
RISCV_TEST1(rv64ui, lh)
RISCV_TEST1(rv64ui, lhu)
RISCV_TEST1(rv64ui, lw)
RISCV_TEST1(rv64ui, lwu)
RISCV_TEST1(rv64ui, ld)
RISCV_TEST1(rv64ui, ld_st)
RISCV_TEST1(rv64ui, lui)
RISCV_TEST1(rv64ui, ma_data)
RISCV_TEST1(rv64ui, or)
RISCV_TEST1(rv64ui, ori)
RISCV_TEST1(rv64ui, sb)
RISCV_TEST1(rv64ui, sh)
RISCV_TEST1(rv64ui, sw)
RISCV_TEST1(rv64ui, sd)
RISCV_TEST1(rv64ui, st_ld)
RISCV_TEST1(rv64ui, sll)
RISCV_TEST1(rv64ui, slli)
RISCV_TEST1(rv64ui, slliw)
RISCV_TEST1(rv64ui, sllw)
RISCV_TEST1(rv64ui, slt)
RISCV_TEST1(rv64ui, slti)
RISCV_TEST1(rv64ui, sltiu)
RISCV_TEST1(rv64ui, sltu)
RISCV_TEST1(rv64ui, sra)
RISCV_TEST1(rv64ui, srai)
RISCV_TEST1(rv64ui, sraiw)
RISCV_TEST1(rv64ui, sraw)
RISCV_TEST1(rv64ui, srl)
RISCV_TEST1(rv64ui, srli)
RISCV_TEST1(rv64ui, srliw)
RISCV_TEST1(rv64ui, srlw)
RISCV_TEST1(rv64ui, sub)
RISCV_TEST1(rv64ui, subw)
RISCV_TEST1(rv64ui, xor)
RISCV_TEST1(rv64ui, xori)

/*
RISCV_TEST1(rv64um, div)
RISCV_TEST1(rv64um, divu)
RISCV_TEST1(rv64um, divuw)
RISCV_TEST1(rv64um, divw)
RISCV_TEST1(rv64um, mul)
RISCV_TEST1(rv64um, mulh)
RISCV_TEST1(rv64um, mulhsu)
RISCV_TEST1(rv64um, mulhu)
RISCV_TEST1(rv64um, mulw)
RISCV_TEST1(rv64um, rem)
RISCV_TEST1(rv64um, remu)
RISCV_TEST1(rv64um, remuw)
RISCV_TEST1(rv64um, remw)
*/

/*
RISCV_TEST1(rv64uf, fadd)
RISCV_TEST1(rv64uf, fdiv)
RISCV_TEST1(rv64uf, fclass)
RISCV_TEST1(rv64uf, fcmp)
RISCV_TEST1(rv64uf, fcvt)
RISCV_TEST1(rv64uf, fcvt_w)
RISCV_TEST1(rv64uf, fmadd)
RISCV_TEST1(rv64uf, fmin)
RISCV_TEST1(rv64uf, ldst)
RISCV_TEST1(rv64uf, move)
RISCV_TEST1(rv64uf, recoding)
*/

/*
RISCV_TEST1(rv64ud, fadd)
RISCV_TEST1(rv64ud, fdiv)
RISCV_TEST1(rv64ud, fclass)
RISCV_TEST1(rv64ud, fcmp)
RISCV_TEST1(rv64ud, fcvt)
RISCV_TEST1(rv64ud, fcvt_w)
RISCV_TEST1(rv64ud, fmadd)
RISCV_TEST1(rv64ud, fmin)
RISCV_TEST1(rv64ud, ldst)
RISCV_TEST1(rv64ud, move)
RISCV_TEST1(rv64ud, structural)
RISCV_TEST1(rv64ud, recoding)
*/

// RISCV_TEST1(rv64uc, rvc)

/*
RISCV_TEST1(rv64ua, amoadd_d)
RISCV_TEST1(rv64ua, amoand_d)
RISCV_TEST1(rv64ua, amomax_d)
RISCV_TEST1(rv64ua, amomaxu_d)
RISCV_TEST1(rv64ua, amomin_d)
RISCV_TEST1(rv64ua, amominu_d)
RISCV_TEST1(rv64ua, amoor_d)
RISCV_TEST1(rv64ua, amoxor_d)
RISCV_TEST1(rv64ua, amoswap_d)
RISCV_TEST1(rv64ua, amoadd_w)
RISCV_TEST1(rv64ua, amoand_w)
RISCV_TEST1(rv64ua, amomax_w)
RISCV_TEST1(rv64ua, amomaxu_w)
RISCV_TEST1(rv64ua, amomin_w)
RISCV_TEST1(rv64ua, amominu_w)
RISCV_TEST1(rv64ua, amoor_w)
RISCV_TEST1(rv64ua, amoxor_w)
RISCV_TEST1(rv64ua, amoswap_w)
RISCV_TEST1(rv64ua, lrsc)
*/

/*
RISCV_TEST1(rv64si, csr)
RISCV_TEST1(rv64si, dirty)
RISCV_TEST2(rv64si, "rv64si", icache_alias, "icache-alias")
RISCV_TEST1(rv64si, ma_fetch)
RISCV_TEST1(rv64si, scall)
RISCV_TEST1(rv64si, wfi)
RISCV_TEST1(rv64si, sbreak)
*/

/*
RISCV_TEST1(rv64mi, breakpoint)
RISCV_TEST1(rv64mi, csr)
RISCV_TEST1(rv64mi, mcsr)
RISCV_TEST1(rv64mi, illegal)
RISCV_TEST1(rv64mi, ma_fetch)
RISCV_TEST1(rv64mi, ma_addr)
RISCV_TEST1(rv64mi, scall)
RISCV_TEST1(rv64mi, sbreak)
RISCV_TEST2(rv64mi, "rv64mi", ld_misaligned, "ld-misaligned")
RISCV_TEST2(rv64mi, "rv64mi", lw_misaligned, "lw-misaligned")
RISCV_TEST2(rv64mi, "rv64mi", lh_misaligned, "lh-misaligned")
RISCV_TEST2(rv64mi, "rv64mi", sh_misaligned, "sh-misaligned")
RISCV_TEST2(rv64mi, "rv64mi", sw_misaligned, "sw-misaligned")
RISCV_TEST2(rv64mi, "rv64mi", sd_misaligned, "sd-misaligned")
RISCV_TEST1(rv64mi, zicntr)
RISCV_TEST1(rv64mi, instret_overflow)
RISCV_TEST1(rv64mi, pmpaddr)
*/

static bool compile_step(char *const *argv) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else if (pid == -1) {
        printf("\033[31m CANCELLED\n");
        perror("fork");
        printf("\033[0m");
        return false;
    }
    int wstatus;
    if (waitpid(pid, &wstatus, 0) < 0) {
        printf("\033[31m CANCELLED\n");
        perror("waitpid");
        printf("\033[0m");
        return false;
    }
    if (WIFSIGNALED(wstatus)) {
        printf("\033[31m CANCELLED\n");
        printf("Child received %s\n", strsignal(WTERMSIG(wstatus)));
        printf("\033[0m");
    } else if (WEXITSTATUS(wstatus) != 0) {
        printf("\033[31m CANCELLED\n");
        printf("Child exit code %d\n", WEXITSTATUS(wstatus));
        printf("\033[0m");
        return false;
    }
    return true;
}

static bool compile_riscv_test(char const *set, char const *test) {
    char const *riscv_test_prefix = getenv("RISCV_TEST_PREFIX");
    if (!riscv_test_prefix) {
        riscv_test_prefix = "";
    }
    char srcpath[64];
    char objpath[64];
    char binpath[64];
    char dirpath[64];
    char gccname[PATH_MAX];
    char copyname[PATH_MAX];
    snprintf(
        srcpath,
        sizeof(srcpath) - 1,
        "riscv-tests/isa/%s/%s.S",
        set,
        test
    );
    snprintf(
        objpath,
        sizeof(objpath) - 1,
        "riscv-tests-build/%s/%s",
        set,
        test
    );
    snprintf(
        binpath,
        sizeof(binpath) - 1,
        "riscv-tests-build/%s/%s.bin",
        set,
        test
    );
    snprintf(dirpath, sizeof(dirpath) - 1, "riscv-tests-build/%s", set);
    snprintf(gccname, sizeof(gccname) - 1, "%sgcc", riscv_test_prefix);
    snprintf(copyname, sizeof(copyname) - 1, "%sobjcopy", riscv_test_prefix);

    if (access(binpath, R_OK) == 0) {
        return true;
    }

    if (!compile_step((char *const[]){
            "mkdir",
            "-p",
            dirpath,
            nullptr,
        })) {
        return false;
    }
    if (!compile_step((char *const[]){
            gccname,
            "-I",
            "riscv-tests/env/p",
            "-I",
            "riscv-tests/isa/macros/scalar",
            "-Tplain.ld",
            "-nodefaultlibs",
            "-nostartfiles",
            "-march=rv64i_zifencei_zicsr",
            "-mabi=lp64",
            "-fno-pic",
            "-static",
            srcpath,
            "-o",
            objpath,
            nullptr,
        })) {
        return false;
    }
    if (!compile_step((char *const[]){
            copyname,
            "-O",
            "binary",
            objpath,
            binpath,
            nullptr,
        })) {
        return false;
    }
    return true;
}

struct riscv_test_state {
    bool     finished, failure;
    uint64_t trap_count;
};

static bool trap_hook(
    void              *cookie,
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    struct rv_trap     trap
) {
    (void)machine;
    struct riscv_test_state *st = cookie;

    if ((trap.cause == RV_CAUSE_ECALL_M || trap.cause == RV_CAUSE_ECALL_S ||
         trap.cause == RV_CAUSE_ECALL_U) &&
        cpu->xregs[17] == 93) {
        if (cpu->xregs[10]) {
            printf(
                "\033[31m Sub-test %" PRIu64 " failed\033[0m\n",
                cpu->xregs[10]
            );
            st->failure = true;
        }
        st->finished = true;
        return false;
    }

    st->trap_count++;
    if (st->trap_count >= 10000) {
        printf(
            "\033[31m VM crash at 0x%" PRIx64 " cause 0x%" PRIx64 "\033[0m\n",
            trap.epc,
            trap.cause
        );
        st->failure  = true;
        st->finished = true;
        return false;
    }

    return true;
}

static bool do_riscv_test(
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    char const        *set,
    char const        *test
) {
    if (!compile_riscv_test(set, test)) {
        return false;
    }

    char pathbuf[PATH_MAX];
    snprintf(
        pathbuf,
        sizeof(pathbuf) - 1,
        "riscv-tests-build/%s/%s.bin",
        set,
        test
    );

    FILE *f = fopen(pathbuf, "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);

    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(len);
    len          = fread(buf, 1, len, f);

    struct riscv_test_state st = {0};

    uint64_t const base_addr = 0x10000;
    machine->ram_start       = base_addr;
    machine->ram_end         = base_addr + len;
    machine->ram             = buf;

    machine->hook_cookie = &st;
    for (int i = 0; i < 32; i++) {
        machine->trap_hook[i] = &trap_hook;
    }

    cpu->privilege = 3;
    cpu->pc        = base_addr;

    while (!st.finished) {
        rv_step_insn(machine, cpu);
    }

    free(machine->ram);
    machine->ram_start = 0;
    machine->ram_end   = 0;

    return !st.failure;
}
