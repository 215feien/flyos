#ifndef MYOS_PMM_H
#define MYOS_PMM_H
#include <stdint.h>
#define PAGE_SIZE 4096
void     pmm_init(uint32_t mb_info_phys);
uint64_t pmm_alloc_page(void);
void     pmm_free_page(uint64_t phys);
uint64_t pmm_total_pages(void);
uint64_t pmm_used_pages(void);
uint64_t pmm_free_pages(void);
#endif
