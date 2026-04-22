// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#include "rv_device.h"
#include "rv_machine.h"
#include "rv_privileged.h"
#include "testcase.h"

#include <string.h>

// Mock MMIO device: a flat 16-byte register file that records the last access.
struct mock_device {
    uint8_t  mem[16];
    uint64_t last_read_offset;
    uint8_t  last_read_size;
    uint64_t last_write_offset;
    uint8_t  last_write_size;
    uint64_t last_write_value;
};

static bool mock_read(void *dev, uint64_t offset, uint8_t size, uint64_t *out) {
    struct mock_device *d = dev;
    d->last_read_offset   = offset;
    d->last_read_size     = size;
    *out                  = 0;
    memcpy(out, d->mem + offset, size);
    return true;
}

static bool mock_write(void *dev, uint64_t offset, uint8_t size, uint64_t value) {
    struct mock_device *d = dev;
    d->last_write_offset  = offset;
    d->last_write_size    = size;
    d->last_write_value   = value;
    memcpy(d->mem + offset, &value, size);
    return true;
}

static bool trap_catch_hook(
    void              *cookie,
    struct rv_machine *machine,
    struct rv_cpu     *cpu,
    struct rv_trap     trap
) {
    (void)machine;
    (void)cpu;
    bool *caught       = cookie;
    caught[trap.cause] = true;
    return true;
}

// MMIO region placed well above RAM (RAM: 0x10000–0x11000).
#define MOCK_BASE UINT64_C(0x20000)
#define MOCK_SIZE 16

// Aligned byte and word reads are dispatched to the correct device callback.
TESTCASE(mmio_aligned_read, {
    struct mock_device dev = {0};
    dev.mem[0]             = 0xAB;
    dev.mem[4]             = 0x78;
    dev.mem[5]             = 0x56;
    dev.mem[6]             = 0x34;
    dev.mem[7]             = 0x12;

    TEST_ASSERT(rv_machine_add_mmio(
        machine,
        (struct rv_mmio_region){
            .base   = MOCK_BASE,
            .size   = MOCK_SIZE,
            .device = &dev,
            .read   = mock_read,
            .write  = mock_write,
        }
    ));

    uint64_t data = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE, &data, 0, RV_ACCESS_LOAD));
    TEST_ASSERT(data == 0xAB);
    TEST_ASSERT(dev.last_read_offset == 0);
    TEST_ASSERT(dev.last_read_size == 1);

    data = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE + 4, &data, 2, RV_ACCESS_LOAD));
    TEST_ASSERT(data == 0x12345678);
    TEST_ASSERT(dev.last_read_offset == 4);
    TEST_ASSERT(dev.last_read_size == 4);
})

// Aligned byte and word writes are dispatched to the correct device callback.
TESTCASE(mmio_aligned_write, {
    struct mock_device dev = {0};

    TEST_ASSERT(rv_machine_add_mmio(
        machine,
        (struct rv_mmio_region){
            .base   = MOCK_BASE,
            .size   = MOCK_SIZE,
            .device = &dev,
            .read   = mock_read,
            .write  = mock_write,
        }
    ));

    uint64_t data = 0xAB;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE, &data, 0, RV_ACCESS_STORE));
    TEST_ASSERT(dev.last_write_offset == 0);
    TEST_ASSERT(dev.last_write_size == 1);
    TEST_ASSERT(dev.last_write_value == 0xAB);
    TEST_ASSERT(dev.mem[0] == 0xAB);

    data = 0x12345678;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE + 4, &data, 2, RV_ACCESS_STORE));
    TEST_ASSERT(dev.last_write_offset == 4);
    TEST_ASSERT(dev.last_write_size == 4);
    TEST_ASSERT(dev.last_write_value == 0x12345678);
    TEST_ASSERT(dev.mem[4] == 0x78);
    TEST_ASSERT(dev.mem[5] == 0x56);
    TEST_ASSERT(dev.mem[6] == 0x34);
    TEST_ASSERT(dev.mem[7] == 0x12);
})

// Access outside both RAM and any MMIO region raises an access fault.
TESTCASE(mmio_fault_hole, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    uint64_t data = 0;
    TEST_ASSERT(!rv_access_phys(machine, cpu, 0x30000, &data, 2, RV_ACCESS_LOAD));
    TEST_ASSERT(caught[RV_CAUSE_LACCESS]);
})

// A device returning false from its callback causes an access fault.
static bool fault_read(void *dev, uint64_t offset, uint8_t size, uint64_t *out) {
    (void)dev;
    (void)offset;
    (void)size;
    (void)out;
    return false;
}

static bool fault_write(void *dev, uint64_t offset, uint8_t size, uint64_t value) {
    (void)dev;
    (void)offset;
    (void)size;
    (void)value;
    return false;
}

TESTCASE(mmio_device_fault, {
    bool caught[32]      = {0};
    machine->hook_cookie = caught;
    for (int i = 0; i < 32; i++) machine->trap_hook[i] = &trap_catch_hook;

    TEST_ASSERT(rv_machine_add_mmio(
        machine,
        (struct rv_mmio_region){
            .base   = MOCK_BASE,
            .size   = MOCK_SIZE,
            .device = nullptr,
            .read   = fault_read,
            .write  = fault_write,
        }
    ));

    uint64_t data = 0;
    TEST_ASSERT(!rv_access_phys(machine, cpu, MOCK_BASE, &data, 2, RV_ACCESS_LOAD));
    TEST_ASSERT(caught[RV_CAUSE_LACCESS]);

    memset(caught, 0, sizeof(caught));
    data = 0xDEAD;
    TEST_ASSERT(!rv_access_phys(machine, cpu, MOCK_BASE, &data, 2, RV_ACCESS_STORE));
    TEST_ASSERT(caught[RV_CAUSE_SACCESS]);
})

// With two regions registered, each access is dispatched to the right device.
TESTCASE(mmio_multiple_regions, {
    struct mock_device dev_a = {0};
    struct mock_device dev_b = {0};
    dev_a.mem[0]             = 0x11;
    dev_b.mem[0]             = 0x22;

    TEST_ASSERT(rv_machine_add_mmio(
        machine,
        (struct rv_mmio_region){
            .base   = MOCK_BASE,
            .size   = MOCK_SIZE,
            .device = &dev_a,
            .read   = mock_read,
            .write  = mock_write,
        }
    ));
    TEST_ASSERT(rv_machine_add_mmio(
        machine,
        (struct rv_mmio_region){
            .base   = MOCK_BASE + 0x1000,
            .size   = MOCK_SIZE,
            .device = &dev_b,
            .read   = mock_read,
            .write  = mock_write,
        }
    ));

    uint64_t data = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE, &data, 0, RV_ACCESS_LOAD));
    TEST_ASSERT(data == 0x11);

    data = 0;
    TEST_ASSERT(rv_access_phys(machine, cpu, MOCK_BASE + 0x1000, &data, 0, RV_ACCESS_LOAD));
    TEST_ASSERT(data == 0x22);
})
