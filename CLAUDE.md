All work in this project MUST have test cases. WOrk in TDD mode always. Before commit all tests MUST succeed. This is critical.

Always work in stages. Commit. then create a new plan a clean context.

Project goal:

To run Linux in emulated RISC-V
More specific feature list includes:
- RV64GC
- Possibly vector in the future
- Supervisor "S-Mode" support
    - Software TLB emulation
- Ability to create custom MMIO devices in the virtual machine, e.g.:
    - Interrupt controllers like the PLIC
    - NS16550A or similar serial chips
    - QEMU VirtIO for disks

I am aiming for strict compliance for the features implemented; instructions that deviate too far from the listings are to be rejected, floating-point math is to be implemented according to the IEEE 754 specification and the RISC-V F and D instruction sets, etc.

`struct rv_machine` is the entire virtual machine; in the future, it will hold one or more CPUs. `struct rv_cpu` is the entire context for one singular virtual core, including registers, CSRs, caches and the TLB. To keep memory access performance high, the access to RAM takes priority and should be in the fast path. Similar fast path tradeoffs should apply to other operations that are more commonly executed.

Architecture considerations:
- Work modular. Do not create cross links between modules unless they're nescesary
- Ask for clarification, ask for specifications. Do not guess too much.
- Follow the patterns in the existing code. Look back to commit 379b414ee0f6e24ad9003f81e64a869ca9e52269 or earlier as a reference. 

Common commands:
- The default Makefile target is `test`; this runs unit tests, all by default
- Set `TESTS=...` to specify which specific tests to run (instead of all)

When I say commit, add/stage the files you just worked on and write a nice commit message with decent detail.
Before commit give me a summary of the most critical bits of the commit. 

A copy of important RISC-V specification documents can be found in ~/Sync/datasheets/ISAs/RISC-V/
