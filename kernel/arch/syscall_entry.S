.section .text
.global syscall_entry
.extern syscall_dispatch

.section .bss
.align 8
.global saved_user_rsp
.global kernel_rsp
.global user_return_rip
.global user_return_rsp
saved_user_rsp:
    .skip 8
kernel_rsp:
    .skip 8
user_return_rip:
    .skip 8
user_return_rsp:
    .skip 8

.section .text
syscall_entry:
    movq %rsp, saved_user_rsp(%rip)
    movq kernel_rsp(%rip), %rsp

    /* 栈上依次：
       r15 r14 r13 r12 rbp rbx r10 r9 r8 rdx rsi rdi rax r11 rcx
       偏移 0   8  16  24  32  40  48 56 64  72  80  88  96 104 112 */
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

    movq 96(%rsp), %rdi     /* nr */
    movq 88(%rsp), %rsi     /* a1 */
    movq 80(%rsp), %rdx     /* a2 */
    movq 72(%rsp), %rcx     /* a3 */

    call syscall_dispatch

    /* 返回值写回 rax 槽（偏移 96） */
    movq %rax, 96(%rsp)

    /* 如果内核设置了 user_return_rip，就覆盖 rcx 槽（偏移 112） */
    movq user_return_rip(%rip), %rax
    testq %rax, %rax
    jz 1f
    movq %rax, 112(%rsp)
    movq $0, user_return_rip(%rip)
1:
    /* 如果内核设置了 user_return_rsp，就覆盖 saved_user_rsp */
    movq user_return_rsp(%rip), %rax
    testq %rax, %rax
    jz 2f
    movq %rax, saved_user_rsp(%rip)
    movq $0, user_return_rsp(%rip)
2:
    /* 现在才 pop，rax 从槽里恢复（是内核的返回值） */
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
    popq %rcx              /* 用户 rip，可能已被覆盖 */

    movq saved_user_rsp(%rip), %rsp
    sysretq