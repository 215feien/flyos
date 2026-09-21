#include "pit.h"
#include <stdint.h>
#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_FREQ 1193182
static inline void outb(uint16_t port,uint8_t val){__asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));}
void pit_init(uint32_t frequency){
    uint32_t divisor=PIT_FREQ/frequency;
    if(divisor==0) divisor=1;
    if(divisor>0xFFFF) divisor=0xFFFF;
    outb(PIT_CMD,0x36);
    outb(PIT_CH0,(uint8_t)(divisor&0xFF));
    outb(PIT_CH0,(uint8_t)((divisor>>8)&0xFF));
}
