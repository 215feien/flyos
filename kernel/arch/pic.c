#include "pic.h"
#include <stdint.h>
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1
static inline void outb(uint16_t port, uint8_t val){__asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));}
static inline uint8_t inb(uint16_t port){uint8_t r;__asm__ volatile("inb %1,%0":"=a"(r):"Nd"(port));return r;}
static inline void io_wait(void){outb(0x80,0);}
void pic_init(void){
    outb(PIC1_CMD,0x11);io_wait();
    outb(PIC2_CMD,0x11);io_wait();
    outb(PIC1_DATA,0x20);io_wait();
    outb(PIC2_DATA,0x28);io_wait();
    outb(PIC1_DATA,0x04);io_wait();
    outb(PIC2_DATA,0x02);io_wait();
    outb(PIC1_DATA,0x01);io_wait();
    outb(PIC2_DATA,0x01);io_wait();
    outb(PIC1_DATA,0xFF);
    outb(PIC2_DATA,0xFF);
}
void pic_send_eoi(int irq){
    if(irq>=8) outb(PIC2_CMD,0x20);
    outb(PIC1_CMD,0x20);
}
void pic_set_mask(int irq){
    uint16_t port=(irq<8)?PIC1_DATA:PIC2_DATA;
    if(irq>=8) irq-=8;
    outb(port,inb(port)|(uint8_t)(1<<irq));
}
void pic_clear_mask(int irq){
    uint16_t port=(irq<8)?PIC1_DATA:PIC2_DATA;
    if(irq>=8) irq-=8;
    outb(port,inb(port)&(uint8_t)~(1<<irq));
}
