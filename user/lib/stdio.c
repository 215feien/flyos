#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include <stdarg.h>

void putchar(char c) {
    sys_write(1, &c, 1);
}

void puts(const char* s) {
    sys_write(1, s, (long)strlen(s));
}

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
    while (i > 0) putchar(buf[--i]);
}

static void print_int(long v, int width, char pad) {
    if (v < 0) {
        putchar('-');
        print_uint((unsigned long)(-v), 10, width > 0 ? width - 1 : 0, pad);
    } else {
        print_uint((unsigned long)v, 10, width, pad);
    }
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') { putchar(*p); continue; }
        p++;

        char pad = ' ';
        int  width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }

        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') p++; }

        switch (*p) {
            case 'c': putchar((char)va_arg(ap, int)); break;
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                puts(s);
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
                puts("0x");
                print_uint(v, 16, 16, '0');
                break;
            }
            case '%': putchar('%'); break;
            default:
                putchar('%');
                if (*p) putchar(*p);
                break;
        }
    }
    va_end(ap);
    return 0;
}

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

#include "syscall.h"

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