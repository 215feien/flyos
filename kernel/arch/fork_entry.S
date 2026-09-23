.section .text
.global fork_child_entry

fork_child_entry:
    movq %r15, %rcx         /* 用户 RIP */
    movq %r14, %rsi         /* 用户 RSP 暂存 rsi（不要改 rsp！） */

    pushq $0x1B             /* SS: user data | RPL3 */
    pushq %rsi              /* RSP = 用户栈 */
    pushq $0x202            /* RFLAGS: IF=1 */
    pushq $0x23             /* CS: user code | RPL3 */
    pushq %rcx              /* RIP */
    movq $0, %rax           /* fork 返回值 = 0 */
    iretq