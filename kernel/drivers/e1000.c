#include "e1000.h"
#include "serial.h"
#include <stdint.h>

/* e1000 寄存器偏移 */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_EEPROM_READ 0x0014
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_IMC       0x00D8
#define E1000_RCTL      0x0100
#define E1000_TCTL      0x0400
#define E1000_RDBAL     0x2800
#define E1000_RDBAH     0x2804
#define E1000_RDLEN     0x2808
#define E1000_RDH       0x2810
#define E1000_RDT       0x2818
#define E1000_TDBAL     0x3800
#define E1000_TDBAH     0x3804
#define E1000_TDLEN     0x3808
#define E1000_TDH       0x3810
#define E1000_TDT       0x3818
#define E1000_MTA       0x5200
#define E1000_RAL       0x5400
#define E1000_RAH       0x5404

static uint64_t mmio = 0;
static uint8_t  mac[6] = {0};

static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)(uintptr_t)(mmio + reg);
}

static inline void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(uintptr_t)(mmio + reg) = val;
}

void e1000_get_mac(uint8_t out[6]) {
    for (int i = 0; i < 6; i++) out[i] = mac[i];
}

void e1000_probe(uint64_t mmio_base) {
    mmio = mmio_base;

    uint32_t status = e1000_read(E1000_STATUS);
    serial_printf("E1000: STATUS = 0x%08x\n", status);

    /* 从 RAL/RAH 读 MAC（QEMU 的 e1000 已经预填） */
    uint32_t ral = e1000_read(E1000_RAL);
    uint32_t rah = e1000_read(E1000_RAH);

    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);

    serial_printf("E1000: MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* 检查 MMIO 是否真的能读——读 CTRL 应该非 0 */
    uint32_t ctrl = e1000_read(E1000_CTRL);
    serial_printf("E1000: CTRL = 0x%08x\n", ctrl);
}