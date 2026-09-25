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
#define SYS_MKDIR   12
#define SYS_CHDIR   13
#define SYS_GETCWD  14
#define SYS_OPENDIR 15
#define SYS_READDIR 16
#define SYS_SPAWN   17
#define SYS_SEM_WAIT 18
#define SYS_SEM_POST 19
#define SYS_EXEC    20
#define SYS_FORK    21
#define SYS_FAT_LS    22
#define SYS_FAT_READ  23
#define SYS_FAT_WRITE   24
#define SYS_FAT_DELETE  25

#endif