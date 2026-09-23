#include "vmm.h"
#include "pmm.h"
#include "serial.h"
#include <stdint.h>

#define ADDR_MASK  (0x000FFFFFFFFFF000ULL)

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static void zero_page(uint64_t phys) {
    uint8_t* p = (uint8_t*)(uintptr_t)phys;
    for (int i = 0; i < 4096; i++) p[i] = 0;
}

static uint64_t* next_table(uint64_t* parent, int idx) {
    if (!(parent[idx] & VMM_PRESENT)) {
        uint64_t np = pmm_alloc_page();
        if (!np) return 0;
        zero_page(np);
        parent[idx] = np | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
    } else {
        /* 已存在的上层页表项也必须有 U，否则用户态无法穿过 */
        parent[idx] |= VMM_USER;
    }
    return (uint64_t*)(uintptr_t)(parent[idx] & ADDR_MASK);
}

static void split_huge_pd(uint64_t* pd, int pd_idx) {
    if (!(pd[pd_idx] & VMM_HUGE)) return;
    uint64_t huge_phys = pd[pd_idx] & ~0x1FFFFFULL;
    uint64_t flags     = pd[pd_idx] & 0xFFF;
    uint64_t pt_phys   = pmm_alloc_page();
    if (!pt_phys) return;
    zero_page(pt_phys);
    uint64_t* pt = (uint64_t*)(uintptr_t)pt_phys;
    for (int i = 0; i < 512; i++) {
        pt[i] = (huge_phys + (uint64_t)i * 4096) | flags | VMM_PRESENT | VMM_USER;
    }
    pd[pd_idx] = pt_phys | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
    invlpg((uint64_t)pd_idx * 0x200000ULL);
}

void vmm_init(void) {
    serial_write("VMM: CR3 = ");
    serial_hex(read_cr3());
    serial_write("\n");
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(uintptr_t)(read_cr3() & ADDR_MASK);
    uint64_t* pdpt = next_table(pml4, (int)pml4_idx); if (!pdpt) return;
    uint64_t* pd   = next_table(pdpt, (int)pdpt_idx); if (!pd)   return;
    split_huge_pd(pd, (int)pd_idx);
    uint64_t* pt   = next_table(pd, (int)pd_idx);     if (!pt)   return;

    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & 0xFFF) | VMM_PRESENT;
    invlpg(virt);
}

void vmm_unmap(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(uintptr_t)(read_cr3() & ADDR_MASK);
    if (!(pml4[pml4_idx] & VMM_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[pml4_idx] & ADDR_MASK);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return;
    uint64_t* pd   = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ADDR_MASK);
    if (!(pd[pd_idx] & VMM_PRESENT)) return;
    if (pd[pd_idx] & VMM_HUGE) return;
    uint64_t* pt   = (uint64_t*)(uintptr_t)(pd[pd_idx] & ADDR_MASK);
    pt[pt_idx] = 0;
    invlpg(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(uintptr_t)(read_cr3() & ADDR_MASK);
    if (!(pml4[pml4_idx] & VMM_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[pml4_idx] & ADDR_MASK);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return 0;
    uint64_t* pd   = (uint64_t*)(uintptr_t)(pdpt[pdpt_idx] & ADDR_MASK);
    if (!(pd[pd_idx] & VMM_PRESENT)) return 0;
    if (pd[pd_idx] & VMM_HUGE) {
        return (pd[pd_idx] & ~0x1FFFFFULL) + (virt & 0x1FFFFFULL);
    }
    uint64_t* pt = (uint64_t*)(uintptr_t)(pd[pd_idx] & ADDR_MASK);
    if (!(pt[pt_idx] & VMM_PRESENT)) return 0;
    return (pt[pt_idx] & ADDR_MASK) + (virt & 0xFFFULL);
}

void vmm_page_fault_handler(struct regs* r) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    serial_write("\n*** PAGE FAULT ***\n");
    serial_write("virt     = "); serial_hex(cr2);        serial_write("\n");
    serial_write("err_code = "); serial_hex(r->err_code); serial_write("\n");
    serial_write("rip      = "); serial_hex(r->rip);      serial_write("\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

uint64_t vmm_current_pml4(void) {
    return read_cr3() & ADDR_MASK;
}

uint64_t vmm_clone_pml4(void) {
    uint64_t old_phys = read_cr3() & ADDR_MASK;
    uint64_t new_phys = pmm_alloc_page();
    if (!new_phys) return 0;

    uint64_t* old_pml4 = (uint64_t*)(uintptr_t)old_phys;
    uint64_t* new_pml4 = (uint64_t*)(uintptr_t)new_phys;

    /* 完整复制 512 项：内核部分（PML4[256+]）和用户部分暂时共享下级页表 */
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    serial_printf("VMM: cloned PML4 0x%lx -> 0x%lx\n", old_phys, new_phys);
    return new_phys;
}