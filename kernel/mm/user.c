#include "user.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include <stdint.h>

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1

static void zero_phys(uint64_t phys) {
    uint8_t* p = (uint8_t*)phys;
    for (int i = 0; i < 4096; i++) p[i] = 0;
}

uint64_t user_load_elf(const uint8_t* elf, uint64_t size) {
    (void)size;
    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)elf;

    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') {
        serial_printf("ELF: bad magic\n");
        return 0;
    }

    serial_printf("ELF: entry=0x%lx, phnum=%u\n",
                  ehdr->e_entry, (uint64_t)ehdr->e_phnum);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph =
            (const Elf64_Phdr*)(elf + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;

        serial_printf("  PT_LOAD vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
                      ph->p_vaddr, ph->p_filesz, ph->p_memsz);

        uint64_t first = ph->p_vaddr & ~0xFFFULL;
        uint64_t last  = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFULL;

        for (uint64_t va = first; va < last; va += 0x1000) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) { serial_printf("ELF: OOM\n"); return 0; }
            zero_phys(phys);
            vmm_map(va, phys, VMM_WRITABLE | VMM_USER);
        }

        uint8_t* dst = (uint8_t*)ph->p_vaddr;
        const uint8_t* src = elf + ph->p_offset;
        for (uint64_t k = 0; k < ph->p_filesz; k++) {
            dst[k] = src[k];
        }
    }

    return ehdr->e_entry;
}

void user_setup_stack(void) {
    uint64_t first = USER_STACK_BASE;
    uint64_t last  = USER_STACK_BASE + USER_STACK_SIZE;

    for (uint64_t va = first; va < last; va += 0x1000) {
        uint64_t phys = pmm_alloc_page();
        zero_phys(phys);
        vmm_map(va, phys, VMM_WRITABLE | VMM_USER);
    }
    serial_printf("USER: stack mapped 0x%lx..0x%lx\n", first, last);
}
