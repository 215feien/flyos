#include "irq.h"
#include "pic.h"

static irq_handler_t handlers[16] = {0};

void irq_init(void) {
    for (int i = 0; i < 16; i++) handlers[i] = 0;
}

void irq_register(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16) handlers[irq] = handler;
}

void irq_handler(struct regs* r) {
    int irq = (int)(r->int_no - 32);
    if (irq < 0 || irq >= 16) return;

    pic_send_eoi(irq);            /* 先 EOI，再调 handler */
    if (handlers[irq]) {
        handlers[irq](r);
    }
}
