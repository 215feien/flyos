#include "pmm.h"
#include "serial.h"
#include <stdint.h>

#define MAX_MEMORY  (4ULL * 1024 * 1024 * 1024)
#define MAX_PAGES   (MAX_MEMORY / PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t  bitmap[BITMAP_SIZE];
static uint64_t total_pages = 0;
static uint64_t used_pages  = 0;
static uint64_t search_hint = 0;

extern char kernel_start[];
extern char kernel_end[];

struct mb_tag { uint32_t type; uint32_t size; };
struct mb_tag_mmap { uint32_t type; uint32_t size; uint32_t entry_size; uint32_t entry_version; };
struct mb_mmap_entry { uint64_t addr; uint64_t len; uint32_t type; uint32_t zero; } __attribute__((packed));

static inline void bitmap_set(uint64_t page)   { bitmap[page >> 3] |=  (uint8_t)(1 << (page & 7)); }
static inline void bitmap_clear(uint64_t page) { bitmap[page >> 3] &= (uint8_t)~(1 << (page & 7)); }
static inline int  bitmap_test(uint64_t page)  { return bitmap[page >> 3] & (1 << (page & 7)); }

static void mark_used(uint64_t start, uint64_t end) {
    uint64_t s = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1ULL);
    uint64_t e =  end               & ~(PAGE_SIZE - 1ULL);
    for (uint64_t a = s; a < e; a += PAGE_SIZE) {
        uint64_t p = a / PAGE_SIZE;
        if (p < total_pages && !bitmap_test(p)) { bitmap_set(p); used_pages++; }
    }
}

void pmm_init(uint32_t mb_info_phys) {
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) bitmap[i] = 0;
    used_pages = 0; search_hint = 0;

    uint8_t* mb   = (uint8_t*)(uintptr_t)mb_info_phys;
    uint32_t size = *(uint32_t*)mb;
    uint8_t* p    = mb + 8;
    uint8_t* end  = mb + size;

    uint64_t max_addr = 0;
    while (p < end) {
        struct mb_tag* tag = (struct mb_tag*)p;
        if (tag->type == 0) break;
        if (tag->type == 6) {
            struct mb_tag_mmap* mm = (struct mb_tag_mmap*)tag;
            uint8_t* e = (uint8_t*)mm + 16;
            uint32_t n = (mm->size - 16) / mm->entry_size;
            for (uint32_t i = 0; i < n; i++) {
                struct mb_mmap_entry* ent = (struct mb_mmap_entry*)e;
                if (ent->type == 1) {
                    uint64_t r_end = ent->addr + ent->len;
                    if (r_end > max_addr) max_addr = r_end;
                }
                e += mm->entry_size;
            }
        }
        p += (tag->size + 7) & ~7U;
    }
    if (max_addr > MAX_MEMORY) max_addr = MAX_MEMORY;
    total_pages = max_addr / PAGE_SIZE;

    serial_write("PMM: total pages = "); serial_hex(total_pages); serial_write("\n");

    for (uint64_t i = 0; i < total_pages; i++) bitmap_set(i);
    used_pages = total_pages;

    p = mb + 8;
    while (p < end) {
        struct mb_tag* tag = (struct mb_tag*)p;
        if (tag->type == 0) break;
        if (tag->type == 6) {
            struct mb_tag_mmap* mm = (struct mb_tag_mmap*)tag;
            uint8_t* e = (uint8_t*)mm + 16;
            uint32_t n = (mm->size - 16) / mm->entry_size;
            for (uint32_t i = 0; i < n; i++) {
                struct mb_mmap_entry* ent = (struct mb_mmap_entry*)e;
                if (ent->type == 1) {
                    uint64_t s = (ent->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1ULL);
                    uint64_t r_end = ent->addr + ent->len;
                    if (r_end > MAX_MEMORY) r_end = MAX_MEMORY;
                    uint64_t e2 = r_end & ~(PAGE_SIZE - 1ULL);
                    for (uint64_t a = s; a + PAGE_SIZE <= e2; a += PAGE_SIZE) {
                        uint64_t pg = a / PAGE_SIZE;
                        if (pg < total_pages && bitmap_test(pg)) { bitmap_clear(pg); used_pages--; }
                    }
                }
                e += mm->entry_size;
            }
        }
        p += (tag->size + 7) & ~7U;
    }

    mark_used(0, 0x100000);
    mark_used((uint64_t)(uintptr_t)kernel_start, (uint64_t)(uintptr_t)kernel_end);
    mark_used((uint64_t)mb_info_phys, (uint64_t)mb_info_phys + size);

    serial_write("PMM: used pages  = "); serial_hex(used_pages);             serial_write("\n");
    serial_write("PMM: free pages  = "); serial_hex(total_pages - used_pages); serial_write("\n");
}

uint64_t pmm_alloc_page(void) {
    for (uint64_t i = search_hint; i < total_pages; i++) {
        if (!bitmap_test(i)) { bitmap_set(i); used_pages++; search_hint = i + 1; return i * PAGE_SIZE; }
    }
    for (uint64_t i = 0; i < search_hint; i++) {
        if (!bitmap_test(i)) { bitmap_set(i); used_pages++; search_hint = i + 1; return i * PAGE_SIZE; }
    }
    return 0;
}

void pmm_free_page(uint64_t phys) {
    uint64_t page = phys / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
        if (page < search_hint) search_hint = page;
    }
}

uint64_t pmm_total_pages(void) { return total_pages; }
uint64_t pmm_used_pages(void)  { return used_pages; }
uint64_t pmm_free_pages(void)  { return total_pages - used_pages; }
