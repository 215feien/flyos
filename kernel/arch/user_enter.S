.section .text
.global user_enter

user_enter:
    movq %rsi, %rsp
    andq $-16, %rsp

    pushq $0x1B
    pushq %rsp
    pushq $0x202
    pushq $0x23
    pushq %rdi
    iretq
