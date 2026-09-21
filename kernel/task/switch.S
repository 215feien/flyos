.section .text
.global task_switch

/* void task_switch(uint64_t* old_rsp_ptr, uint64_t new_rsp); */
task_switch:
    pushfq
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popfq
    ret
