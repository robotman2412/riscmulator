// Copyright © 2026, __robotAtPLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Callback types for memory-mapped I/O device access.
// `offset` is relative to the region base address; `size` is in bytes (1/2/4/8).
// Return false to signal an access fault; the dispatch layer raises the trap.
typedef bool (*rv_mmio_read_fn_t)(void *dev, uint64_t offset, uint8_t size, uint64_t *out);
typedef bool (*rv_mmio_write_fn_t)(void *dev, uint64_t offset, uint8_t size, uint64_t value);

// A contiguous memory-mapped I/O region registered with a machine.
struct rv_mmio_region {
    uint64_t           base;
    uint64_t           size;
    void              *device;
    rv_mmio_read_fn_t  read;
    rv_mmio_write_fn_t write;
};
