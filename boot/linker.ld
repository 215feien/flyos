ENTRY(_start)

SECTIONS
{
    . = 1M;

    kernel_start = .;

    .multiboot_header : {
        KEEP(*(.multiboot_header))
    }

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    .userelf : {
        *(.userelf)
    }

    kernel_end = .;
}
