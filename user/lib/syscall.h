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
i64  sys_mkdir (const char* path);
i64  sys_chdir (const char* path);
i64  sys_getcwd(char* buf, i64 max);
i64  sys_opendir(const char* path);
i64  sys_readdir(int fd, char* name, i64 max);
i64  sys_spawn (void);
i64 sys_sem_wait(int id);
i64 sys_sem_post(int id);
i64 sys_exec(void);
i64 sys_fork(void);
i64 sys_fat_ls(char* buf, i64 max);
i64 sys_fat_read(const char* name, void* buf, i64 max);

#endif
