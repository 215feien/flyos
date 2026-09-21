#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

typedef long i64;

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

i64  sys_write (int fd, const void* buf, i64 n);
i64  sys_read  (int fd, void* buf,       i64 n);
void sys_exit  (int code);
i64  sys_open  (const char* name);
i64  sys_close (int fd);
i64  sys_fread (int fd, void* buf, i64 n);
i64  sys_fwrite(int fd, const void* buf, i64 n);
i64  sys_ls    (char* buf, i64 max);
i64  sys_unlink(const char* name);
i64  sys_sync  (void);
i64  sys_sleep (i64 ms);

#endif