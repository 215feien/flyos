#ifndef MYOS_VMM_H
#define MYOS_VMM_H

#include <stdint.h>
#include "idt.h"

#define VMM_PRESENT   (1ULL << 0)
#define VMM_WRITABLE  (1ULL << 1)
#define VMM_USER      (1ULL << 2)
#define VMM_HUGE      (1ULL << 7)
#define VMM_NX        (1ULL << 63)

void     vmm_init(void);
void     vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void     vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
void     vmm_page_fault_handler(struct regs* r);
uint64_t vmm_clone_pml4(void);
uint64_t vmm_current_pml4(void);

#endif
