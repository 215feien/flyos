#include "serial.h"
#include <stdint.h>
#include <stdarg.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, (uint8_t)c);
}

void serial_write(const char* s) {
    while (*s) serial_putc(*s++);
}

void serial_hex(uint64_t v) {
    static const char hex[] = "0123456789ABCDEF";
    serial_write("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(v >> i) & 0xF]);
    }
}

void serial_puts_dec(uint64_t v) {
    if (v == 0) { serial_putc('0'); return; }
    char buf[24];
    int i = 0;
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) serial_putc(buf[--i]);
}

static void print_uint(uint64_t v, int base, int width, char pad) {
    char buf[32];
    int i = 0;
    const char* digits = "0123456789abcdef";

    if (v == 0) buf[i++] = '0';
    while (v > 0) {
        buf[i++] = digits[v % (uint64_t)base];
        v /= (uint64_t)base;
    }
    while (i < width) buf[i++] = pad;
    while (i > 0) serial_putc(buf[--i]);
}

static void print_int(int64_t v, int width, char pad) {
    if (v < 0) {
        serial_putc('-');
        print_uint((uint64_t)(-v), 10, width > 0 ? width - 1 : 0, pad);
    } else {
        print_uint((uint64_t)v, 10, width, pad);
    }
}

void serial_vprintf(const char* fmt, va_list ap) {
    for (const char* p = fmt; *p; p++) {
        if (*p != '%') { serial_putc(*p); continue; }
        p++;

        char pad = ' ';
        int  width = 0;
        if (*p == '0') { pad = '0'; p++; }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }

        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; if (*p == 'l') p++; }

        switch (*p) {
            case 'c': {
                char c = (char)va_arg(ap, int);
                serial_putc(c);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                while (*s) serial_putc(*s++);
                break;
            }
            case 'd':
            case 'i': {
                int64_t v = is_long ? va_arg(ap, int64_t)
                                    : (int64_t)va_arg(ap, int);
                print_int(v, width, pad);
                break;
            }
            case 'u': {
                uint64_t v = is_long ? va_arg(ap, uint64_t)
                                     : (uint64_t)va_arg(ap, unsigned int);
                print_uint(v, 10, width, pad);
                break;
            }
            case 'x': {
                uint64_t v = is_long ? va_arg(ap, uint64_t)
                                     : (uint64_t)va_arg(ap, unsigned int);
                print_uint(v, 16, width, pad);
                break;
            }
            case 'p': {
                uint64_t v = (uint64_t)va_arg(ap, void*);
                serial_write("0x");
                print_uint(v, 16, 16, '0');
                break;
            }
            case '%': {
                serial_putc('%');
                break;
            }
            default:
                serial_putc('%');
                if (*p) serial_putc(*p);
                break;
        }
    }
}

void serial_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
}