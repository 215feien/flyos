#include "mouse.h"
#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "serial.h"
#include "fb_cursor.h"
#include <stdint.h>

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static volatile uint8_t buttons = 0;

uint8_t mouse_buttons(void) {
    return buttons;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int mouse_cycle = 0;
static uint8_t mouse_byte[3];

static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(PS2_CMD, 0xD4);
    mouse_wait_write();
    outb(PS2_DATA, data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(PS2_DATA);
}

static void mouse_handler(struct regs* r) {
    (void)r;

    if (!(inb(PS2_STATUS) & 0x20)) return;

    uint8_t data = inb(PS2_DATA);

    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) return;
            mouse_byte[0] = data;
            mouse_cycle = 1;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle = 2;
            break;
                case 2: {
            mouse_byte[2] = data;
            mouse_cycle = 0;

            buttons = mouse_byte[0] & 0x07;   /* ← 记录按键状态 */

            int dx = (int8_t)mouse_byte[1];
            int dy = (int8_t)mouse_byte[2];

            fb_cursor_move(dx / 2, -dy / 2);
            break;
        }
    }
}

void mouse_init(void) {
    mouse_wait_write();
    outb(PS2_CMD, 0xA8);

    mouse_wait_write();
    outb(PS2_CMD, 0x20);
    mouse_wait_read();
    uint8_t status = inb(PS2_DATA);
    status |= 0x02;
    status &= (uint8_t)~0x20;

    mouse_wait_write();
    outb(PS2_CMD, 0x60);
    mouse_wait_write();
    outb(PS2_DATA, status);

    mouse_write(0xF6);
    (void)mouse_read();
    mouse_write(0xF4);
    (void)mouse_read();

    irq_register(12, mouse_handler);
    pic_clear_mask(2);
    pic_clear_mask(12);

    serial_printf("MOUSE: initialized\n");
}