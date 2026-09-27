.section .text
.global fork_child_entry

fork_child_entry:
    movq %r15, %rcx
    movq %r14, %rsi

    pushq $0x1B
    pushq %rsi
    pushq $0x202
    pushq $0x23
    pushq %rcx
    movq $0, %rax
    iretq
