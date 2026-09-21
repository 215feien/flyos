#ifndef USER_STDLIB_H
#define USER_STDLIB_H

#include "string.h"

void* malloc(size_t n);
void  free(void* p);
void  exit(int code);

#endif
