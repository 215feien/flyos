#include "vmm.h"
#include "idt.h"
#include "serial.h"
#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void (*const isr_table[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void (*const irq_table[16])(void) = {
    irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
};

static void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt[i] = (struct idt_entry){0};
    }
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_table[i]);
    }
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, (uint64_t)irq_table[i]);
    }
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void exception_handler(struct regs* r) {
    static const char* names[32] = {
        "DE Divide Error", "DB Debug", "NMI", "BP Breakpoint",
        "OF Overflow", "BR Bound Range", "UD Invalid Opcode", "NM Device N/A",
        "DF Double Fault", "CSO Coprocessor", "TS Invalid TSS", "NP Segment",
        "SS Stack Fault", "GP General Protection", "PF Page Fault", "RES",
        "MF x87 FP", "AC Alignment", "MC Machine Check", "XM SIMD FP",
        "VE Virtualization", "CP Control Protection", "RES", "RES",
        "RES", "RES", "RES", "RES",
        "HV Hypervisor", "VC VMM Comm", "SX Security", "RES"
    };

    if (r->int_no == 14) vmm_page_fault_handler(r);
    serial_write("\n*** EXCEPTION ***\n");
    if (r->int_no < 32) {
        serial_write(names[r->int_no]);
        serial_write("\n");
    }
    serial_write("int_no   = "); serial_hex(r->int_no);   serial_write("\n");
    serial_write("err_code = "); serial_hex(r->err_code); serial_write("\n");
    serial_write("rip      = "); serial_hex(r->rip);      serial_write("\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
