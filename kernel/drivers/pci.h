#ifndef MYOS_PCI_H
#define MYOS_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint8_t  irq_line;
    uint32_t bar[6];
} pci_device_t;

void         pci_init(void);
pci_device_t* pci_find(uint16_t vendor, uint16_t device);

/* 配置空间读写 */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif