# Implementation Plan: RV64GC Emulator → Boot Linux

Goal from CLAUDE.md: run Linux on emulated RISC-V (RV64GC + S-Mode + MMIO devices).

Rules:
- TDD always — write tests first, all tests must pass before committing.
- Work in stages. After each stage: commit, then start a fresh context with a new plan.
- Follow existing code patterns. Reference commit 379b414 or earlier for style.
- Ask before guessing on specs.

## Architecture Constraints (from CLAUDE.md)

**Strict spec compliance.** Instructions that deviate from the RISC-V specification must be
rejected with an illegal-instruction trap — no silent approximations. Floating-point must be
implemented to IEEE 754 and the RISC-V F/D ISA exactly (correct NaN canonicalization,
rounding modes, exception flags, FMIN/FMAX NaN rules, etc.).

**Struct responsibilities:**
- `struct rv_machine` — the entire virtual machine. Holds RAM, the CPU array
  (`cpus` / `cpu_count`), MMIO regions, and all machine-wide synchronization primitives
  (`atomic_lock`, `reservations[]`).
- `struct rv_cpu` — one virtual core. Holds integer/FP registers, all CSRs, hart ID, halted
  flag, and also the **TLB and any other per-core caches**. Do not put TLB or per-core caches
  in `rv_machine`.

**Fast path.** RAM access must stay on the fast path. Any new layer (MMU translation, MMIO
dispatch) must be structured so that the common case — a RAM hit — adds minimal overhead
(e.g. TLB hit before page-table walk, RAM range check before MMIO scan).

---

## ~~M Extension (Multiply/Divide)~~ ✓ DONE

---

## ~~C Extension (Compressed Instructions)~~ ✓ DONE

---

## ~~Multi-CPU Infrastructure~~ ✓ DONE

---

## ~~A Extension (Atomics)~~ ✓ DONE

---

## ~~F Extension (Single-Precision Floating Point)~~ ✓ DONE

---

## ~~D Extension (Double-Precision Floating Point)~~ ✓ DONE

---

## Memory Access Layer Refactor (rv_mem)

**Why now:** Before adding PMP and Sv39 paging, the RAM access layer must be restructured so
page-boundary-crossing and eventually address translation can be handled cleanly in one place.
The current `RV_READ_RAM` / `RV_WRITE_RAM` macros perform a single pointer-cast and cannot
split accesses that cross a page boundary (where virtual→physical translation may yield
non-contiguous physical pages).

### Design decisions

- **`cpu` parameter added now** — `rv_mem_read/write` take `struct rv_cpu *cpu` even though
  it is currently unused (`(void)cpu`). This avoids a second refactor when Sv39 is added and
  `cpu->csr.satp` / privilege level are needed for translation.
- **Unsigned outputs; caller sign-extends** — functions always write to `uint8/16/32/64_t *`.
  LB/LH/LW sign extension is done by the load instruction handlers, not the memory layer.
- **Page boundary splitting** — any multi-byte access that straddles a page boundary is split
  at the boundary into two independently translated sub-accesses. This is legal per the
  RISC-V spec and correctly handles the case where the two pages map to non-contiguous physical
  memory.
- **`phys_read/write` internals** — raw RAM range check plus `memcpy`; returns `false` on OOB.
  Future: will also dispatch to MMIO when address is outside RAM.
- **Atomics are always naturally aligned** — the atomics code enforces natural alignment before
  reaching the memory layer, so aligned 4/8-byte accesses never cross a page boundary (page
  size 4 KiB is a multiple of 8). No special handling needed there.

### Tasks

1. Write tests in `test/src/mem.c`:
   - `mem_basic` — direct `rv_mem_read/write` round-trips for all sizes within one page.
   - `mem_page_boundary` — uint16/32/64 writes and reads that cross the page boundary at
     `0x2000` (machine RAM = 2 pages from `0x1000`); verify per-byte layout and read-back.
   - `mem_oob` — accesses before RAM start, after RAM end, and straddling the end return `false`.
2. Create `emulator/include/rv_mem.h` declaring:
   `rv_mem_read8/16/32/64(machine, cpu, addr, out*)` and `rv_mem_write8/16/32/64(...)`.
   Also define `RV_PAGE_SIZE 4096` here.
3. Create `emulator/src/rv_mem.c` with static helpers `phys_read/write` and `mem_read/write`
   (page-split logic), then the eight public functions.
4. Add `'src/rv_mem.c'` to `emulator_src` in `emulator/meson.build`.
5. Remove `RV_READ_RAM` / `RV_WRITE_RAM` from `emulator/include/rv_machine.h`.
6. Update call sites:
   - `rv_cpu.c`: two `RV_READ_RAM` → `rv_mem_read16` for instruction fetch.
   - `rv_impl/base.c`: all load `RV_READ_RAM` (with explicit sign/zero casts) and store
     `RV_WRITE_RAM` → typed `rv_mem_read/write`.
   - `rv_impl/atomics.c`: LR, SC, and AMO read/write macros → `rv_mem_read/write32/64`.
7. All existing and new tests must pass.

---

## Privileged Mode Hardening (rv64mi + rv64si)

**Why:** Linux requires correct M-mode and S-mode behavior. This stage validates that before adding virtual memory.

### Tasks
1. Uncomment rv64mi and rv64si test blocks in `riscv_tests.c`.
2. Fix / implement anything the tests expose. Known gaps:
   - **WFI** (`SYSTEM` opcode, funct12=0x105): set `cpu->halted = true` and let the thread
     block; the run loop should re-check for pending interrupts before resuming. A simple
     yield (`sched_yield`) is acceptable initially.
   - **MRET / SRET**: restore privilege level from MPP/SPP, restore MIE/SIE from MPIE/SPIE, set MPIE/SPIE=1, set MPP=U (or M for MRET). Verify current implementation in `rv_privileged.c`.
   - **Performance counters**: `instret`, `cycle`, `time` — increment `instret` in `rv_step_insn()`. Wire `time` to host clock if needed.
   - **PMP (Physical Memory Protection)**: rv64mi pmpaddr tests require at least stub PMP CSR support (pmpcfg0, pmpaddr0-15). A no-op implementation that accepts writes and reads them back is acceptable initially.
   - **Misaligned access traps**: verify that misaligned load/store raises `LOAD_MISALIGNED` / `STORE_MISALIGNED` cause correctly.
   - **Illegal instruction trap**: make sure unknown opcodes and bad CSR accesses trap with `ILLEGAL_INSN` cause.
3. All rv64mi and rv64si tests must pass.

---

## Virtual Memory: Sv39

**Why:** Linux on RISC-V uses the Sv39 paging scheme. Without it, the kernel cannot boot.

### Spec reference: RISC-V Privileged ISA §4.4 (Sv39)

### Tasks
1. Add a function `rv_translate_addr(machine, cpu, vaddr, access_type) → (paddr, trap_or_ok)` in a new file `emulator/src/rv_mmu.c` / `emulator/include/rv_mmu.h`.
   - `access_type`: instruction fetch, load, store (for correct fault cause codes: `IPAGEFAULT`, `LPAGEFAULT`, `SPAGEFAULT`).
   - Read `satp` CSR: if `MODE=0` (bare), return `vaddr` unchanged.
   - If `MODE=8` (Sv39): 3-level page table walk using PPN from `satp`.
   - Page table entries are 64-bit. Check V bit, R/W/X bits, U bit (vs current privilege), A/D bits.
   - On fault: return a trap struct with appropriate cause.
2. Add a **software TLB cache** in `struct rv_cpu` (not `rv_machine` — TLB is per-core state):
   - Simple direct-mapped or small fully-associative array of `(vpn, ppn, flags)` entries.
   - `SFENCE.VMA` flushes the TLB (implement in `rv_base_miscmem` or a new system handler).
   - On TLB miss: walk page table, insert entry.
3. Replace all `RV_READ_RAM` / `RV_WRITE_RAM` call sites in the CPU with a new `rv_mem_read()` / `rv_mem_write()` inline that keeps RAM on the fast path:
   - If `satp.MODE == 0` (bare) or privilege is M-mode: skip TLB, go directly to RAM/MMIO check.
   - Otherwise: check TLB first (fast path on hit); on miss, walk page table, insert entry, then proceed.
   - RAM range check comes before MMIO scan — MMIO is only reached when the address is outside RAM.
4. Write custom tests (not from riscv-tests) that:
   - Set up a simple page table in RAM.
   - Enable Sv39 via `satp`.
   - Access a mapped page and verify it reads/writes correctly.
   - Access an unmapped page and verify a page fault trap is raised.

---

## MMIO Infrastructure

**Why:** Devices like PLIC, UART, and VirtIO are memory-mapped. The machine needs a way to dispatch reads/writes at specific address ranges to device callbacks.

### Tasks
1. Define a device interface in a new `emulator/include/rv_device.h`:
   ```c
   typedef uint64_t (*rv_mmio_read_fn_t) (void *dev, uint64_t offset, uint8_t size);
   typedef void     (*rv_mmio_write_fn_t)(void *dev, uint64_t offset, uint8_t size, uint64_t value);

   struct rv_mmio_region {
       uint64_t          base, size;
       void             *device;
       rv_mmio_read_fn_t  read;
       rv_mmio_write_fn_t write;
   };
   ```
2. Add `struct rv_mmio_region *mmio; size_t mmio_count;` to `struct rv_machine`.
3. Add `rv_machine_add_mmio(machine, region)` in `rv_machine.c`.
4. Update `rv_mem_read()` / `rv_mem_write()` (from Sv39 stage): after address translation, if the address falls outside RAM, scan MMIO regions and dispatch. If no match, generate an access fault.
5. Write a test with a mock MMIO device that records reads/writes and verify dispatch.

---

## PLIC (Platform-Level Interrupt Controller)

**Why:** Linux uses PLIC for all external interrupts (UART RX, disk, etc.).

### Spec reference: RISC-V PLIC specification.

### Tasks
1. Create `emulator/src/rv_plic.c` / `emulator/include/rv_plic.h`.
2. Implement the PLIC register map as an MMIO device (base address: `0x0C000000`, standard QEMU virt layout):
   - Priority registers (per source): `0x0000004 * source`
   - Pending array: `0x001000`
   - Enable bits (per context): `0x002000 + 0x80 * context`
   - Priority threshold + claim/complete (per context): `0x200000 + 0x1000 * context`
3. On claim: return the highest-priority pending+enabled interrupt ID. Clear pending.
4. On complete: mark interrupt as completable (allows re-triggering).
5. Wire PLIC interrupt signaling to each CPU's `mip.SEIP` (external interrupt pending bit).
   PLIC contexts map to harts: since `rv_machine` will eventually hold multiple CPUs, design
   the PLIC to iterate `machine->cpus[i]` when raising/clearing SEIP — do not hard-code a
   single `cpu` pointer inside the device.
6. Write tests: register a mock interrupt source, trigger it, verify claim returns correct ID.

---

## NS16550A UART

**Why:** Linux console output and input. This is what lets you see the kernel boot log.

### Tasks
1. Create `emulator/src/rv_uart.c` / `emulator/include/rv_uart.h`.
2. Implement the NS16550A register map as an MMIO device (base: `0x10000000`, standard):
   - `RBR/THR` (offset 0): RX buffer / TX holding register.
   - `IER` (offset 1): interrupt enable.
   - `IIR/FCR` (offset 2): interrupt identification / FIFO control.
   - `LCR` (offset 3): line control.
   - `MCR` (offset 4): modem control.
   - `LSR` (offset 5): line status — always set THRE (TX ready) bit; set DR (data ready) when input pending.
   - `MSR` (offset 6): modem status.
3. TX write → write byte to stdout (or a configurable fd).
4. RX: read from stdin (non-blocking via `poll()`). Queue in a small ring buffer.
5. Raise PLIC interrupt (source 10, standard virt layout) when RX data arrives.
6. Write a test that writes to THR and verifies byte appears on output; and that LSR.THRE is always set.

---

## VirtIO Block Device

**Why:** Linux needs a disk to load the root filesystem from.

### Spec reference: VirtIO 1.2 specification, §5.2 Block Device.

### Tasks
1. Create `emulator/src/rv_virtio_blk.c` / `emulator/include/rv_virtio_blk.h`.
2. Implement VirtIO MMIO transport (base: `0x10001000`, standard virt layout):
   - Magic value, version, device ID (2 = block), vendor ID registers.
   - Status register protocol: driver writes ACKNOWLEDGE → DRIVER → FEATURES_OK → DRIVER_OK.
   - Feature negotiation: support at minimum `VIRTIO_BLK_F_SIZE_MAX`, `VIRTIO_BLK_F_SEG_MAX`.
3. Implement one virtqueue (queue 0):
   - Descriptor table, available ring, used ring at guest-provided addresses (via `QueueDescLow/High`, `QueueDriverLow/High`, `QueueDeviceLow/High`).
   - On `QueueNotify` write: process all available descriptors, execute read/write on the backing image file.
4. Raise PLIC interrupt (source 1) on completion.
5. Accept a file path as the backing disk image (raw format initially).
6. Write tests with a small synthetic disk image; verify read request returns correct data.

---

## OpenSBI / SBI Shim + Linux Boot

**Why:** Linux expects an SBI (Supervisor Binary Interface) firmware in M-mode to handle platform calls (console putchar, timer, IPI, etc.). Options: (a) run OpenSBI as the M-mode firmware, (b) implement a minimal SBI shim directly in the emulator.

**Recommendation:** implement a minimal SBI shim in the emulator (simpler, no dependency on OpenSBI binary).

### Tasks
1. Create `emulator/src/rv_sbi.c` / `emulator/include/rv_sbi.h`.
2. Install an ECALL trap hook that intercepts ECALL from S-mode and handles SBI calls (a7 = extension ID).
   The SBI shim receives the calling `cpu` and the `machine`; it must not assume a single hart:
   - `sbi_console_putchar` (legacy, EID=1): write `a0` byte to stdout.
   - `sbi_console_getchar` (legacy, EID=2): read from stdin.
   - `sbi_set_timer` (EID=0, FID=0 / TIME extension): set the calling hart's `mtimecmp` to `a0`;
     clear STIP, set MTIP when timer fires.
   - `sbi_hart_start` (HSM, EID=0x48534D): validate target hart ID is in range; set the
     target CPU's PC and a1 register, clear its `halted` flag so its thread resumes.
   - `sbi_hart_stop` (HSM): halt the calling hart (set a stopped flag on `cpu`).
   - `sbi_send_ipi`: no-op for single hart.
   - `sbi_system_reset` (SRST extension): exit emulator.
3. Implement a timer: after N instructions (or using host `clock_gettime`), set `mip.MTIP`; if `mie.MTIE` is set, deliver timer interrupt; SBI clears by writing to `mtimecmp`.
4. Generate a Flattened Device Tree (FDT/DTB) describing:
   - 1 hart (RISC-V, RV64GC)
   - RAM region
   - PLIC at `0x0C000000`
   - UART at `0x10000000`
   - VirtIO block at `0x10001000`
   - Chosen node: `bootargs`, `stdout-path = "/uart@10000000"`, `rng-seed`.
5. Load the kernel image (e.g., `Image` from a Linux build) at `0x80200000`; place DTB at `0x80000000`. Set `a0=0` (hartid), `a1=DTB address`, jump to kernel entry in S-mode.
6. Boot test: kernel prints "Linux version …" to UART, mounts initrd, reaches a shell.

---

## Stage Summary

| Feature                      | Tests                     | Status  |
|------------------------------|---------------------------|---------|
| M extension                  | rv64um (all)              | ✓ done  |
| C extension                  | rv64uc rvc                | ✓ done  |
| Multi-CPU + pthreads         | custom multi-cpu tests    | ✓ done  |
| A extension                  | rv64ua (all)              | ✓ done  |
| F extension                  | rv64uf (all)              | ✓ done  |
| D extension                  | rv64ud (all)              | ✓ done  |
| Privileged hardening         | rv64mi + rv64si           |         |
| Sv39 virtual memory          | custom MMU tests          |         |
| MMIO infrastructure          | mock device test          |         |
| PLIC                         | PLIC unit tests           |         |
| UART NS16550A                | UART unit tests           |         |
| VirtIO block                 | virtio unit tests         |         |
| SBI + Linux boot             | boot integration test     |         |

---

## Notes & Open Questions

- **Toolchain**: `riscv64-linux-gnu-` prefix is used in tests.
- **FP compliance**: Berkeley SoftFloat is used for IEEE 754 compliance (imported in F/D stages).
- **Strict illegal-instruction enforcement**: Any opcode or funct3/funct7 combination not
  explicitly listed in the ISA must call `rv_do_iillegal()`. Audit each new implementation
  file as it is written; do not add a `default: /* ignore */` fallthrough.
- **Timer source**: For SBI timer, decide between instruction-count-based timer (deterministic,
  easy) vs host `CLOCK_MONOTONIC` (accurate). Linux needs the timer to fire; accuracy matters
  less for initial boot.
- **VirtIO version**: Use MMIO transport v2 (legacy MMIO is simpler but Linux may not probe it
  without DTS hints). Confirm in DTB which transport to advertise.
- **Linux config**: Use a minimal `defconfig` + `CONFIG_SERIAL_8250=y`,
  `CONFIG_VIRTIO_BLK=y`, `CONFIG_RISCV_SBI_V01=y`.
- **Multi-CPU**: `rv_machine` holds `struct rv_cpu *cpus; size_t cpu_count;`.
  Avoid embedding `cpu` pointers in device structs; always pass `machine` + `mhartid` index
  so devices remain correct with multiple CPUs.
- **Atomics design**: settled on a single `pthread_mutex_t atomic_lock` in
  `rv_machine` covering all LR/SC/AMO operations. No host intrinsics. Regular stores do not
  cancel LR reservations — acceptable for Linux workloads.
