
    .global _start
    .option norvc
_start:
    la s0, msg
    j 1f
0:
    # Print character.
    li a7, 1
    ecall
    addi s0, s0, 1
1:
    lbu a0, 0(s0)
    bnez a0, 0b
    
    # Exit
    li a7, 2
    ecall
    
msg:
    .ascii "Hello, World!\n"
    .byte 0
