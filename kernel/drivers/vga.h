#ifndef MYOS_VGA_H
#define MYOS_VGA_H

#include <stdint.h>

void     vga_clear(void);
void     vga_putc(char c);
void     vga_write(const char* s);
uint16_t vga_get_cell(int row, int col);
void     vga_set_cell(int row, int col, uint16_t value);

#endif