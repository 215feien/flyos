#ifndef MYOS_PANIC_H
#define MYOS_PANIC_H

#include <stdint.h>

void panic(const char* fmt, ...) __attribute__((noreturn));
void panic_assert_fail(const char* expr, const char* file, int line) __attribute__((noreturn));

#define ASSERT(cond) do { \
    if (!(cond)) { \
        panic_assert_fail(#cond, __FILE__, __LINE__); \
    } \
} while (0)

#endif