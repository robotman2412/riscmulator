
#include "rv_cpu.h"
#include "rv_machine.h"

#include <assert.h>

int main() {
    struct rv_machine machine = {0};
    struct rv_cpu     cpu     = {0};

    rv_forcefeed_insn(&machine, &cpu, 0x2401e06f);
    assert(cpu.pc == 123456);

    cpu.pc = 0;
    rv_forcefeed_insn(&machine, &cpu, 0x42ba206f);
    assert(cpu.pc == 666666);

    cpu.pc = 0;
    rv_forcefeed_insn(&machine, &cpu, 0x23ef406f);
    assert(cpu.pc == 999998);

    cpu.pc = 0;
    rv_forcefeed_insn(&machine, &cpu, 0x3ee8406f);
    assert(cpu.pc == 541678);

    return 0;
}
