#ifndef MYOS_HEAP_H
#define MYOS_HEAP_H

#include <stddef.h>

void  heap_init(void);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void  kfree(void* ptr);
void  heap_stats(void);

#endif
