#include "vmm.h"
#include "pmm.h"
#include "serial.h"
#include "fb.h"
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

static inline void flush_tlb_all(void) {
    uint64_t cr3 = read_cr3();
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static void zero_page(uint64_t phys) {
    uint8_t* p = (uint8_t*)(uintptr_t)phys;
    for (int i = 0; i < 4096; i++) p[i] = 0;
}

/* 取得或创建下一级页表。
   已存在的项也要补 U 位——否则用户态无法穿过中间层。 */
static uint64_t* next_table(uint64_t* parent, int idx) {
    if (!(parent[idx] & VMM_PRESENT)) {
        uint64_t np = pmm_alloc_page();
        if (!np) return 0;
        zero_page(np);
        parent[idx] = np | VMM_PRESENT | VMM_WRITABLE | VMM_USER;
    } else {
        /* 已存在的上层页表项也必须有 U */
        parent[idx] |= VMM_USER;
    }
    return (uint64_t*)(uintptr_t)(parent[idx] & ADDR_MASK);
}

/* 如果 PD 项是 2MB 大页，拆成 512 个 4KB 小页 */
static int split_huge_pd(uint64_t* pd, int pd_idx) {
    if (!(pd[pd_idx] & VMM_HUGE)) return 0;   /* 不是大页，无需拆 */

    uint64_t huge_phys = pd[pd_idx] & ~0x1FFFFFULL;
    uint64_t flags     = pd[pd_idx] & 0xFFF;

    uint64_t pt_phys = pmm_alloc_page();
    if (!pt_phys) return -1;
    zero_page(pt_phys);
    uint64_t* pt = (uint64_t*)(uintptr_t)pt_phys;

    for (int i = 0; i < 512; i++) {
        pt[i] = (huge_phys + (uint64_t)i * 4096) | flags | VMM_PRESENT | VMM_USER;
    }
    pd[pd_idx] = pt_phys | VMM_PRESENT | VMM_WRITABLE | VMM_USER;

    /* 刷新整个 TLB（invlpg 只刷 1 个 4KB 页，不够） */
    flush_tlb_all();
    return 0;
}

void vmm_init(void) {
    serial_printf("VMM: CR3 = ");
    serial_hex(read_cr3());
    serial_printf("\n");
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(uintptr_t)(read_cr3() & ADDR_MASK);
    uint64_t* pdpt = next_table(pml4, (int)pml4_idx); if (!pdpt) return;
    uint64_t* pd   = next_table(pdpt, (int)pdpt_idx); if (!pd)   return;

    if (split_huge_pd(pd, (int)pd_idx) < 0) {
        serial_printf("VMM: WARN: split_huge_pd failed at virt=0x%lx\n", virt);
        return;
    }

    if (pd[pd_idx] & VMM_HUGE) {
        serial_printf("VMM: WARN: still huge at virt=0x%lx\n", virt);
        return;
    }

    uint64_t* pt = next_table(pd, (int)pd_idx); if (!pt) return;

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

uint64_t vmm_current_pml4(void) {
    return read_cr3() & ADDR_MASK;
}

/* ===== 浅拷贝：只复制 PML4 顶层，下级页表共享 ===== */
uint64_t vmm_clone_pml4(void) {
    uint64_t old_phys = read_cr3() & ADDR_MASK;
    uint64_t new_phys = pmm_alloc_page();
    if (!new_phys) return 0;

    uint64_t* old_pml4 = (uint64_t*)(uintptr_t)old_phys;
    uint64_t* new_pml4 = (uint64_t*)(uintptr_t)new_phys;

    for (int i = 0; i < 512; i++) {
        new_pml4[i] = old_pml4[i];
    }

    serial_printf("VMM: cloned PML4 0x%lx -> 0x%lx\n", old_phys, new_phys);
    return new_phys;
}

/* ===== 深拷贝：复制用户空间（低半区），内核空间共享 ===== */
static uint64_t copy_page(uint64_t src_phys) {
    uint64_t dst_phys = pmm_alloc_page();
    if (!dst_phys) return 0;
    uint8_t* dst = (uint8_t*)(uintptr_t)dst_phys;
    uint8_t* src = (uint8_t*)(uintptr_t)src_phys;
    for (int i = 0; i < 4096; i++) dst[i] = src[i];
    return dst_phys;
}

uint64_t vmm_clone_pml4_deep(uint64_t src_pml4_phys) {
    uint64_t dst_pml4_phys = pmm_alloc_page();
    if (!dst_pml4_phys) return 0;
    zero_page(dst_pml4_phys);

    uint64_t* src_pml4 = (uint64_t*)(uintptr_t)src_pml4_phys;
    uint64_t* dst_pml4 = (uint64_t*)(uintptr_t)dst_pml4_phys;

    for (int i = 0; i < 512; i++) {
        /* 高半区内核空间：共享 */
        if (i >= 256) {
            dst_pml4[i] = src_pml4[i];
            continue;
        }
        /* 低半区用户空间：深拷贝 */
        if (!(src_pml4[i] & VMM_PRESENT)) {
            dst_pml4[i] = 0;
            continue;
        }
        uint64_t src_pdpt_phys = src_pml4[i] & ADDR_MASK;
        uint64_t dst_pdpt_phys = pmm_alloc_page();
        if (!dst_pdpt_phys) return 0;
        zero_page(dst_pdpt_phys);

        uint64_t* src_pdpt = (uint64_t*)(uintptr_t)src_pdpt_phys;
        uint64_t* dst_pdpt = (uint64_t*)(uintptr_t)dst_pdpt_phys;
        dst_pml4[i] = dst_pdpt_phys | (src_pml4[i] & 0xFFF);

        for (int j = 0; j < 512; j++) {
            if (!(src_pdpt[j] & VMM_PRESENT)) { dst_pdpt[j] = 0; continue; }

            uint64_t src_pd_phys = src_pdpt[j] & ADDR_MASK;
            uint64_t dst_pd_phys = pmm_alloc_page();
            if (!dst_pd_phys) return 0;
            zero_page(dst_pd_phys);

            uint64_t* src_pd = (uint64_t*)(uintptr_t)src_pd_phys;
            uint64_t* dst_pd = (uint64_t*)(uintptr_t)dst_pd_phys;
            dst_pdpt[j] = dst_pd_phys | (src_pdpt[j] & 0xFFF);

            for (int k = 0; k < 512; k++) {
                if (!(src_pd[k] & VMM_PRESENT)) { dst_pd[k] = 0; continue; }
                /* 2MB 大页：直接共享（内核恒等映射会走这里，但我们只处理低半区，
                   低半区用户空间通常是 4KB 页） */
                if (src_pd[k] & VMM_HUGE) {
                    dst_pd[k] = src_pd[k];
                    continue;
                }
                uint64_t src_pt_phys = src_pd[k] & ADDR_MASK;
                uint64_t dst_pt_phys = pmm_alloc_page();
                if (!dst_pt_phys) return 0;
                zero_page(dst_pt_phys);

                uint64_t* src_pt = (uint64_t*)(uintptr_t)src_pt_phys;
                uint64_t* dst_pt = (uint64_t*)(uintptr_t)dst_pt_phys;
                dst_pd[k] = dst_pt_phys | (src_pd[k] & 0xFFF);

                for (int l = 0; l < 512; l++) {
                    if (!(src_pt[l] & VMM_PRESENT)) { dst_pt[l] = 0; continue; }
                    uint64_t src_frame = src_pt[l] & ADDR_MASK;

                    /* framebuffer 共享，不复制 */
                    fb_info_t* fbi = fb_get_info();
                    if (fbi && fbi->addr) {
                        uint64_t fb_phys = fbi->addr;
                        uint64_t fb_size = (uint64_t)fbi->pitch * fbi->height;
                        if (src_frame >= fb_phys && src_frame < fb_phys + fb_size) {
                            dst_pt[l] = src_pt[l];   /* 直接共享 */
                            continue;
                        }
                    }

                    uint64_t dst_frame = copy_page(src_frame);
                    if (!dst_frame) return 0;
                    dst_pt[l] = dst_frame | (src_pt[l] & 0xFFF);
                }
            }
        }
    }

    serial_printf("VMM: deep-cloned PML4 0x%lx -> 0x%lx\n",
                  src_pml4_phys, dst_pml4_phys);
    return dst_pml4_phys;
}

void vmm_page_fault_handler(struct regs* r) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    serial_printf("\n*** PAGE FAULT ***\n");
    serial_printf("virt     = "); serial_hex(cr2);        serial_write("\n");
    serial_printf("err_code = "); serial_hex(r->err_code); serial_write("\n");
    serial_printf("  P     = ");
    serial_puts_dec((r->err_code & 1) ? 1 : 0);
    serial_write(" (0=not-present, 1=protection)\n");
    serial_printf("  W/R   = ");
    serial_puts_dec((r->err_code & 2) ? 1 : 0);
    serial_write(" (0=read, 1=write)\n");
    serial_printf("  U/S   = ");
    serial_puts_dec((r->err_code & 4) ? 1 : 0);
    serial_write(" (0=kernel, 1=user)\n");
    serial_printf("rip      = "); serial_hex(r->rip);      serial_write("\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}