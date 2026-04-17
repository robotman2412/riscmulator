
#include "rv_cpu.h"
#include "rv_machine.h"
#include "rv_privileged.h"

#include <stdio.h>
#include <stdlib.h>

static bool ecall_hook(void *cookie, struct rv_machine *machine, struct rv_cpu *cpu, struct rv_trap trap) {
    (void)cookie;
    (void)machine;
    if (trap.cause == RV_CAUSE_ECALL_M && cpu->xregs[17] == 1) {
        fputc(cpu->xregs[10], stdout);
        fflush(stdout);
        return false;
    }
    if (trap.cause == RV_CAUSE_ECALL_M && cpu->xregs[17] == 2) {
        exit(cpu->xregs[10]);
        return false;
    }
    return true;
}

int main() {
    FILE *f = fopen("helloworld_10000.bin", "rb");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);

    fseek(f, 0, SEEK_SET);
    uint8_t *buf   = malloc(len);
    long     count = fread(buf, 1, len, f);
    if (count != len) {
        perror("fread");
        return 1;
    }

    struct rv_machine machine = {0};
    struct rv_cpu     cpu     = {0};

    uint64_t const base_addr = 0x10000;
    machine.ram_start        = base_addr;
    machine.ram_end          = base_addr + len;
    machine.ram              = buf;

    machine.trap_hook[RV_CAUSE_ECALL_M] = &ecall_hook;

    cpu.privilege = 3;
    cpu.pc        = base_addr;

    while (1) {
        rv_step_insn(&machine, &cpu);
    }
}
