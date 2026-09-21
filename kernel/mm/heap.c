#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"
#include <stdint.h>

#define HEAP_START   0xFFFF800000000000UL
#define HEAP_PAGES   512
#define HEAP_SIZE    ((uint64_t)HEAP_PAGES * PAGE_SIZE)

#define ALIGN_UP(x, a)  (((x) + (a) - 1) & ~((uint64_t)(a) - 1))
#define ALIGN16(x)      ALIGN_UP(x, 16)

typedef struct block {
    uint64_t size;
    uint64_t is_free;
    struct block* next;
    struct block* prev;
} block_t;

#define HEADER_SIZE  ((uint64_t)sizeof(block_t))

static block_t* head;

void heap_init(void) {
    for (int i = 0; i < HEAP_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            serial_printf("HEAP: out of physical pages at %d\n", i);
            break;
        }
        vmm_map(HEAP_START + (uint64_t)i * PAGE_SIZE, phys, VMM_WRITABLE);
    }
    head = (block_t*)HEAP_START;
    head->size = HEAP_SIZE;
    head->is_free = 1;
    head->next = 0;
    head->prev = 0;
    serial_printf("HEAP: 0x%lx - 0x%lx (%lu bytes)\n",
                  (uint64_t)HEAP_START,
                  (uint64_t)(HEAP_START + HEAP_SIZE),
                  HEAP_SIZE);
}

static void merge_all(void) {
    block_t* b = head;
    while (b && b->next) {
        if (b->is_free && b->next->is_free &&
            (uint64_t)b + b->size == (uint64_t)b->next) {
            b->size += b->next->size;
            b->next = b->next->next;
            if (b->next) b->next->prev = b;
            continue;
        }
        b = b->next;
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;
    uint64_t need = ALIGN16((uint64_t)size + HEADER_SIZE);
    if (need < HEADER_SIZE + 16) need = HEADER_SIZE + 16;

    block_t* b = head;
    while (b) {
        if (b->is_free && b->size >= need) {
            if (b->size >= need + HEADER_SIZE + 64) {
                block_t* nb = (block_t*)((uint64_t)b + need);
                nb->size   = b->size - need;
                nb->is_free = 1;
                nb->next   = b->next;
                nb->prev   = b;
                if (b->next) b->next->prev = nb;
                b->next = nb;
                b->size = need;
            }
            b->is_free = 0;
            return (void*)((uint64_t)b + HEADER_SIZE);
        }
        b = b->next;
    }
    serial_printf("HEAP: OOM for %lu bytes\n", (uint64_t)size);
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;
    uint64_t addr = (uint64_t)ptr;
    if (addr < HEAP_START + HEADER_SIZE || addr >= HEAP_START + HEAP_SIZE) {
        serial_printf("HEAP: bad free 0x%lx\n", addr);
        return;
    }
    block_t* b = (block_t*)(addr - HEADER_SIZE);
    if (b->is_free) {
        serial_printf("HEAP: double free 0x%lx\n", addr);
        return;
    }
    b->is_free = 1;
    merge_all();
}

void* kzalloc(size_t size) {
    void* p = kmalloc(size);
    if (p) {
        uint8_t* q = (uint8_t*)p;
        for (size_t i = 0; i < size; i++) q[i] = 0;
    }
    return p;
}

void heap_stats(void) {
    uint64_t total = 0, free = 0;
    int blocks = 0, free_blocks = 0;
    block_t* b = head;
    while (b) {
        total += b->size;
        blocks++;
        if (b->is_free) { free += b->size; free_blocks++; }
        b = b->next;
    }
    serial_printf("HEAP: total %lu, free %lu, blocks %d (%d free)\n",
                  total, free, blocks, free_blocks);
}
