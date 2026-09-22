#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include <stdarg.h>

/* ===== 不加锁的底层输出 ===== */
static void raw_putchar(char c) {
    sys_write(1, &c, 1);
}

static void raw_puts(const char* s) {
    sys_write(1, s, (long)strlen(s));
}

/* ===== 带锁的对外接口 ===== */
void putchar(char c) {
    sys_sem_wait(0);
    raw_putchar(c);
    sys_sem_post(0);
}

void puts(const char* s) {
    sys_sem_wait(0);
    raw_puts(s);
    sys_sem_post(0);
}

/* ===== printf 内部用 raw 版本，避免嵌套锁 ===== */
static void print_uint(unsigned long v, int base, int width, char pad) {
    char buf[32];
    int i = 0;
    const char* digits = "0123456789abcdef";
    if (v == 0) buf[i++] = '0';
    while (v > 0) {
        buf[i++] = digits[v % (unsigned long)base];
        v /= (unsigned long)base;
    }
    while (i < width) buf[i++] = pad;
    while (i > 0) raw_putchar(buf[--i]);
}

static void print_int(long v, int width, char pad) {
    if (v < 0) {
        raw_putchar('-');
        print_uint((unsigned long)(-v), 10, width > 0 ? width - 1 : 0, pad);
    } else {
        print_uint((unsigned long)v, 10, width, pad);
    }
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') { raw_putchar(*p); continue; }
        p++;

        char pad = ' ';
        int  width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }

        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') p++; }

        switch (*p) {
            case 'c': raw_putchar((char)va_arg(ap, int)); break;
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                raw_puts(s);
                break;
            }
            case 'd':
            case 'i': {
                long v = is_long ? va_arg(ap, long)
                                 : (long)va_arg(ap, int);
                print_int(v, width, pad);
                break;
            }
            case 'u': {
                unsigned long v = is_long ? va_arg(ap, unsigned long)
                                          : (unsigned long)va_arg(ap, unsigned int);
                print_uint(v, 10, width, pad);
                break;
            }
            case 'x': {
                unsigned long v = is_long ? va_arg(ap, unsigned long)
                                          : (unsigned long)va_arg(ap, unsigned int);
                print_uint(v, 16, width, pad);
                break;
            }
            case 'p': {
                unsigned long v = (unsigned long)va_arg(ap, void*);
                raw_puts("0x");
                print_uint(v, 16, 16, '0');
                break;
            }
            case '%': raw_putchar('%'); break;
            default:
                raw_putchar('%');
                if (*p) raw_putchar(*p);
                break;
        }
    }
    va_end(ap);
    return 0;
}

/* ===== 输入 ===== */
char getchar(void) {
    char c;
    sys_read(0, &c, 1);
    return c;
}

int readline(char* buf, int max) {
    int len = 0;
    for (;;) {
        char c = getchar();
        if (c == '\r' || c == '\n') {
            putchar('\n');
            buf[len] = 0;
            return len;
        }
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                putchar('\b'); putchar(' '); putchar('\b');
            }
            continue;
        }
        if (c < 32 || c > 126) continue;
        if (len >= max - 1) continue;
        buf[len++] = c;
        putchar(c);
    }
}

/* ===== 文件系统（不变） ===== */
int fs_open(const char* name) {
    return (int)sys_open(name);
}
int fs_close(int fd) {
    return (int)sys_close(fd);
}
long fs_read(int fd, void* buf, long n) {
    return (long)sys_fread(fd, buf, n);
}
long fs_write(int fd, const void* buf, long n) {
    return (long)sys_fwrite(fd, buf, n);
}
long fs_ls(char* buf, long max) {
    return (long)sys_ls(buf, max);
}
int fs_unlink(const char* name) {
    return (int)sys_unlink(name);
}
int fs_sync(void) {
    return (int)sys_sync();
}

int fs_mkdir(const char* path) {
    return (int)sys_mkdir(path);
}
int fs_chdir(const char* path) {
    return (int)sys_chdir(path);
}
int fs_getcwd(char* buf, int max) {
    return (int)sys_getcwd(buf, max);
}
long fs_ls_path(const char* path, char* buf, long max) {
    int fd = (int)sys_opendir(path);
    if (fd < 0) return -1;
    long pos = 0;
    char name[64];
    while (1) {
        long r = sys_readdir(fd, name, sizeof(name));
        if (r <= 0) break;
        long l = 0;
        while (name[l]) l++;
        if (pos + l + 1 > max) break;
        for (long i = 0; i < l; i++) buf[pos++] = name[i];
        buf[pos++] = '\n';
    }
    sys_close(fd);
    return pos;
}

/* ===== 锁 ===== */
void lock_stdout(void) {
    sys_sem_wait(0);
}

void unlock_stdout(void) {
    sys_sem_post(0);
}
