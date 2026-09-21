.set MAGIC, 0xE85250D6
.set ARCH, 0

.section .multiboot_header, "a"
.align 8
multiboot_header_start:
    .long MAGIC
    .long ARCH
    .long multiboot_header_end - multiboot_header_start
    .long -(MAGIC + ARCH + (multiboot_header_end - multiboot_header_start))
    .short 0
    .short 0
    .long 8
multiboot_header_end:

.section .text
.code32
.global _start
_start:
    cli
    mov $stack_top, %esp
    mov %ebx, %edi
    mov %eax, %esi

    mov $pml4, %eax
    mov $pdpt, %ebx
    or $0x3, %ebx
    mov %ebx, (%eax)

    mov $pdpt, %eax
    mov $pd, %ebx
    or $0x3, %ebx
    mov %ebx, (%eax)

    mov $pd, %eax
    mov $0x83, %ebx
    mov $512, %ecx
1:
    mov %ebx, (%eax)
    add $0x200000, %ebx
    add $8, %eax
    loop 1b

    mov $pml4, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    lgdt gdt64_ptr

    ljmp $0x08, $long_mode

.code64
long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $stack_top, %rsp
    and $-16, %rsp
    and $-16, %rsp

    mov %edi, %edi
    mov %esi, %esi
    call kmain

1:  hlt
    jmp 1b

.section .rodata
.align 8
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
stack_bottom:
    .skip 16384
stack_top:
