#ifndef MYOS_USER_H
#define MYOS_USER_H

#include <stdint.h>

#define USER_STACK_BASE 0x0000000070000000UL
#define USER_STACK_SIZE 0x4000UL

uint64_t user_load_elf(const uint8_t* elf, uint64_t size);
void     user_setup_stack(void);

#endif
