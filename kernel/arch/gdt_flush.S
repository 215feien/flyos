.section .text
.global gdt_flush
.global tss_flush

gdt_flush:
    lgdt (%rdi)

    mov $0x08, %rax
    pushq %rax
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ret

tss_flush:
    mov $0x28, %ax
    ltr %ax
    ret
