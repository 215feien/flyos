#include "vga.h"
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR  0x0F

static volatile uint16_t* const vga_buffer = (uint16_t*)0xB8000;
static int vga_row = 0;
static int vga_col = 0;

static void vga_putentry(int row, int col, char c) {
    vga_buffer[row * VGA_WIDTH + col] = (uint16_t)(uint8_t)c | (VGA_COLOR << 8);
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (uint16_t)' ' | (VGA_COLOR << 8);
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
        } else if (vga_row > 0) {
            vga_row--;
            vga_col = VGA_WIDTH - 1;
        }
        vga_putentry(vga_row, vga_col, ' ');
        return;
    } else {
        vga_putentry(vga_row, vga_col, c);
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (uint16_t)' ' | (VGA_COLOR << 8);
        }
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_write(const char* s) {
    while (*s) vga_putc(*s++);
}

uint16_t vga_get_cell(int row, int col) {
    return vga_buffer[row * VGA_WIDTH + col];
}

void vga_set_cell(int row, int col, uint16_t value) {
    vga_buffer[row * VGA_WIDTH + col] = value;
}