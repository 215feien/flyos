.section .text
.global syscall_entry
.extern syscall_dispatch

.section .bss
.align 8
.global saved_user_rsp
.global kernel_rsp
.global user_return_rip
.global user_return_rsp
.global syscall_user_rcx
saved_user_rsp:    .skip 8
kernel_rsp:        .skip 8
user_return_rip:   .skip 8
user_return_rsp:   .skip 8
syscall_user_rcx:  .skip 8

.section .text
syscall_entry:
    movq %rsp, saved_user_rsp(%rip)
    movq kernel_rsp(%rip), %rsp

    pushq %rcx
    pushq %r11
    pushq %rax
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 先保存用户 RIP（rcx）到全局 */
    movq %rcx, syscall_user_rcx(%rip)

    /* 再读参数：rdi=nr, rsi=a1, rdx=a2, rcx=a3 */
    movq 96(%rsp), %rdi
    movq 88(%rsp), %rsi
    movq 80(%rsp), %rdx
    movq 72(%rsp), %rcx

    call syscall_dispatch

    movq %rax, 96(%rsp)

    /* exec 用：覆盖用户 RIP */
    movq user_return_rip(%rip), %rax
    testq %rax, %rax
    jz 1f
    movq %rax, 112(%rsp)
    movq $0, user_return_rip(%rip)
1:
    /* exec 用：覆盖用户 RSP */
    movq user_return_rsp(%rip), %rax
    testq %rax, %rax
    jz 2f
    movq %rax, saved_user_rsp(%rip)
    movq $0, user_return_rsp(%rip)
2:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rax
    popq %r11
    popq %rcx

    movq saved_user_rsp(%rip), %rsp
    sysretq
