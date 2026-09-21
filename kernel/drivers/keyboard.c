#include "keyboard.h"
#include "irq.h"
#include "idt.h"
#include "serial.h"
#include <stdint.h>

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_BUF_SIZE 256

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char keymap[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0,  ' ',
    /* 其余为功能键，自动为 0 */
};

static int shift_pressed = 0;

static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile int  kbd_head = 0;
static volatile int  kbd_tail = 0;

static void kbd_push(char c) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) return;   /* 满，丢弃 */
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

int keyboard_getchar_nonblock(void) {
    if (kbd_head == kbd_tail) return -1;
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return (int)c;
}

static void keyboard_handler(struct regs* r) {
    (void)r;

    /* 鼠标数据（bit 5 = 1）留给 mouse_handler 处理 */
    if (inb(KBD_STATUS) & 0x20) return;

    uint8_t scancode = inb(KBD_DATA);

    /* 松开键：最高位 1 */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_pressed = 0;
        return;
    }

    /* Shift 按下 */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    if (scancode < 128) {
        char c = keymap[scancode];
        if (c) {
            if (shift_pressed && c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            kbd_push(c);
        }
    }
}

void keyboard_init(void) {
    irq_register(1, keyboard_handler);
    /* 清空键盘输出缓冲区，避免残留数据 */
    while (inb(KBD_STATUS) & 0x01) {
        (void)inb(KBD_DATA);
    }
}