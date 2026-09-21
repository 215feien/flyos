#include "gdt.h"
#include "serial.h"
#include <stdint.h>

#define GDT_ENTRIES 7

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;
static struct tss_entry tss;

extern void gdt_flush(uint64_t gdtp_ptr);
extern void tss_flush(void);

static void set_gate(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[i].limit_low  = limit & 0xFFFF;
    gdt[i].base_low   = base & 0xFFFF;
    gdt[i].base_mid   = (base >> 16) & 0xFF;
    gdt[i].access     = access;
    gdt[i].flags      = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[i].base_high  = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    set_gate(0, 0, 0,      0x00, 0x00);
    set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0);
    set_gate(2, 0, 0xFFFFF, 0x92, 0xC0);
    set_gate(3, 0, 0xFFFFF, 0xF2, 0xC0);
    set_gate(4, 0, 0xFFFFF, 0xFA, 0xA0);

    uint64_t tss_base  = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_entry) - 1;

    gdt[5].limit_low = tss_limit & 0xFFFF;
    gdt[5].base_low  = tss_base & 0xFFFF;
    gdt[5].base_mid  = (tss_base >> 16) & 0xFF;
    gdt[5].access    = 0x89;
    gdt[5].flags     = (tss_limit >> 16) & 0x0F;
    gdt[5].base_high = (tss_base >> 24) & 0xFF;

    *(uint64_t*)&gdt[6] = (tss_base >> 32) & 0xFFFFFFFFULL;

    uint8_t* p = (uint8_t*)&tss;
    for (uint64_t i = 0; i < sizeof(tss); i++) p[i] = 0;
    tss.iomap_base = sizeof(struct tss_entry);

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)gdt;

    gdt_flush((uint64_t)&gdtp);
    tss_flush();

    serial_printf("GDT: loaded, TSS base=0x%lx\n", tss_base);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
