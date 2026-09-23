#ifndef MYOS_USER_H
#define MYOS_USER_H

#include <stdint.h>

#define USER_STACK_BASE       0x0000000070000000UL
#define USER_STACK_SIZE       0x4000UL

#define CHILD_CODE_BASE       0x0000000000800000UL   /* hello 程序 */
#define CHILD_STACK_BASE      0x000000007F000000UL
#define CHILD_STACK_SIZE      0x4000UL

uint64_t user_load_elf(const uint8_t* elf, uint64_t size);
void     user_setup_stack(void);
void     user_setup_stack_at(uint64_t base, uint64_t size);
uint64_t user_fork_space(uint64_t parent_pml4_phys);

#endif