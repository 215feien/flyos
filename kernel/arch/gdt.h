#ifndef MYOS_GDT_H
#define MYOS_GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#define SEL_KCODE  0x08
#define SEL_KDATA  0x10
#define SEL_UDATA  0x18
#define SEL_UCODE  0x20
#define SEL_TSS    0x28

#endif
