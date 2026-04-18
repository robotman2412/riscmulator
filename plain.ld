OUTPUT_FORMAT(elf64-littleriscv)
OUTPUT_ARCH(riscv)

ENTRY(_start)

PHDRS
{
    text     PT_LOAD    FLAGS(0x05); /* Execute + Read */
    rodata   PT_LOAD    FLAGS(0x04); /* Read only */
    data     PT_LOAD    FLAGS(0x06); /* Write + Read */
}

SECTIONS
{
    . = 0x10000;

    . = ALIGN(4096);
    .text : {
        *(.text._start)
        *(.text .text.*)
    } :text

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    } :rodata

    . = ALIGN(4096);
    .got : { *(.got) } :data
    .got.plt : { *(.got.plt) } :data
    .tohost : {
        *(.tohost .tohost.*)
    } :data
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data
    PROVIDE(__global_pointer$ = .);

    .bss : {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /* Discard useless sections. */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.interp)
    }
}
