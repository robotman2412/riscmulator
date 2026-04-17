OUTPUT_FORMAT(elf64-littleriscv)
OUTPUT_ARCH(riscv)

ENTRY(_start)

PHDRS
{
    text     PT_LOAD    FLAGS(0x05); /* Execute + Read */
    rodata   PT_LOAD    FLAGS(0x04); /* Read only */
    data     PT_LOAD    FLAGS(0x06); /* Write + Read */
    dynamic  PT_DYNAMIC FLAGS(0x06); /* Dynamic PHDR for relocations */
}

SECTIONS
{
    . = 0x10000;

    . = ALIGN(256);
    .interrupt_vector_table : {
        *(.interrupt_vector_table)
    } :text
    .text : {
        *(.text.riscv_exception_asm)
        *(.text.__trap_asm)
        *(.text .text.*)
    } :text
    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    } :rodata
    .dynstr : { *(.dynstr) } :rodata
    .dynsym : { *(.dynsym) } :rodata
    .rela.dyn : { *(.rela.dyn) } :rodata
    . = ALIGN(4096);

    .dynamic : { *(.dynamic) } :data :dynamic
    .got : { *(.got) } :data
    .got.plt : { *(.got.plt) } :data
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data
    PROVIDE(__global_pointer$ = .);

    /* Add zero-initialized memory last to avoid padding. */
    .bss : {
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    } :data
    . = ALIGN(4096);

    /* Discard useless sections. */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
        *(.interp)
    }
}
