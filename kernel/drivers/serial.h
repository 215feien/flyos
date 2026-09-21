#ifndef MYOS_SERIAL_H
#define MYOS_SERIAL_H

#include <stdint.h>
#include <stdarg.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* s);
void serial_hex(uint64_t v);
void serial_puts_dec(uint64_t v);
void serial_printf(const char* fmt, ...);
void serial_vprintf(const char* fmt, va_list ap);
void mouse_init(void);
uint8_t mouse_buttons(void);

#endif