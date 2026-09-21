#include "stdlib.h"
#include "syscall.h"

#define HEAP_SIZE (64 * 1024)

static char   heap[HEAP_SIZE];
static size_t heap_top = 0;

void* malloc(size_t n) {
    n = (n + 15) & ~15UL;
    if (heap_top + n > HEAP_SIZE) return 0;
    void* p = &heap[heap_top];
    heap_top += n;
    return p;
}

void free(void* p) {
    (void)p;
}

void exit(int code) {
    sys_exit(code);
}
