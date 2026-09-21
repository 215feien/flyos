#ifndef MYOS_SYSCALL_H
#define MYOS_SYSCALL_H

#include <stdint.h>

void     syscall_init(void);
uint64_t syscall_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3);

#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_READ    3
#define SYS_OPEN    4
#define SYS_CLOSE   5
#define SYS_FREAD   6
#define SYS_FWRITE  7
#define SYS_LS      8
#define SYS_UNLINK  9
#define SYS_SYNC    10
#define SYS_SLEEP   11

#endif
