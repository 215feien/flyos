#ifndef MYOS_IRQ_H
#define MYOS_IRQ_H
#include "idt.h"
typedef void (*irq_handler_t)(struct regs*);
void irq_init(void);
void irq_register(int irq, irq_handler_t handler);
void irq_handler(struct regs* r);
#endif
