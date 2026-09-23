#include "syscall.h"

static i64 syscall3(i64 n, i64 a1, i64 a2, i64 a3) {
    i64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

i64 sys_write(int fd, const void* buf, i64 n) {
    return syscall3(SYS_WRITE, fd, (i64)buf, n);
}

i64 sys_read(int fd, void* buf, i64 n) {
    return syscall3(SYS_READ, fd, (i64)buf, n);
}

void sys_exit(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

i64 sys_open(const char* name) {
    return syscall3(SYS_OPEN, (i64)name, 0, 0);
}

i64 sys_close(int fd) {
    return syscall3(SYS_CLOSE, fd, 0, 0);
}

i64 sys_fread(int fd, void* buf, i64 n) {
    return syscall3(SYS_FREAD, fd, (i64)buf, n);
}

i64 sys_fwrite(int fd, const void* buf, i64 n) {
    return syscall3(SYS_FWRITE, fd, (i64)buf, n);
}

i64 sys_ls(char* buf, i64 max) {
    return syscall3(SYS_LS, (i64)buf, max, 0);
}

i64 sys_unlink(const char* name) {
    return syscall3(SYS_UNLINK, (i64)name, 0, 0);
}

i64 sys_sync(void) {
    return syscall3(SYS_SYNC, 0, 0, 0);
}

i64 sys_sleep(i64 ms) {
    return syscall3(SYS_SLEEP, ms, 0, 0);
}

i64 sys_mkdir(const char* path) {
    return syscall3(SYS_MKDIR, (i64)path, 0, 0);
}

i64 sys_chdir(const char* path) {
    return syscall3(SYS_CHDIR, (i64)path, 0, 0);
}

i64 sys_getcwd(char* buf, i64 max) {
    return syscall3(SYS_GETCWD, (i64)buf, max, 0);
}

i64 sys_opendir(const char* path) {
    return syscall3(SYS_OPENDIR, (i64)path, 0, 0);
}

i64 sys_readdir(int fd, char* name, i64 max) {
    return syscall3(SYS_READDIR, fd, (i64)name, max);
}

i64 sys_spawn(void) {
    return syscall3(SYS_SPAWN, 0, 0, 0);
}

i64 sys_sem_wait(int id) {
    return syscall3(SYS_SEM_WAIT, id, 0, 0);
}

i64 sys_sem_post(int id) {
    return syscall3(SYS_SEM_POST, id, 0, 0);
}
i64 sys_exec(void) {
    return syscall3(SYS_EXEC, 0, 0, 0);
}

i64 sys_fork(void) {
    return syscall3(SYS_FORK, 0, 0, 0);
}

i64 sys_fat_ls(char* buf, i64 max) {
    return syscall3(SYS_FAT_LS, (i64)buf, max, 0);
}

i64 sys_fat_read(const char* name, void* buf, i64 max) {
    return syscall3(SYS_FAT_READ, (i64)name, (i64)buf, max);
}