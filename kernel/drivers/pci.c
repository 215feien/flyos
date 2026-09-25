#include "pci.h"
#include "serial.h"
#include <stdint.h>

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

#define MAX_PCI_DEVICES  64

static pci_device_t devices[MAX_PCI_DEVICES];
static int device_count = 0;

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint32_t pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((1u << 31)
                    | ((uint32_t)bus  << 16)
                    | ((uint32_t)slot << 11)
                    | ((uint32_t)func << 8)
                    | (offset & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset & 0xFC);
    return (uint16_t)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_read32(bus, slot, func, offset & 0xFC);
    return (uint8_t)((v >> ((offset & 3) * 8)) & 0xFF);
}

static void probe(uint8_t bus, uint8_t slot, uint8_t func) {
    if (device_count >= MAX_PCI_DEVICES) return;

    uint32_t id = pci_read32(bus, slot, func, 0x00);
    uint16_t vendor = id & 0xFFFF;
    if (vendor == 0xFFFF) return;

    pci_device_t* d = &devices[device_count++];
    d->bus        = bus;
    d->slot       = slot;
    d->func       = func;
    d->vendor_id  = vendor;
    d->device_id  = (id >> 16) & 0xFFFF;

    uint32_t classreg = pci_read32(bus, slot, func, 0x08);
    d->prog_if    = (classreg >> 8)  & 0xFF;
    d->subclass   = (classreg >> 16) & 0xFF;
    d->class_code = (classreg >> 24) & 0xFF;

    uint32_t hdr = pci_read32(bus, slot, func, 0x0C);
    d->header_type = (hdr >> 16) & 0xFF;

    uint32_t irq = pci_read32(bus, slot, func, 0x3C);
    d->irq_line = irq & 0xFF;

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_read32(bus, slot, func, 0x10 + i * 4);
    }

    serial_printf("PCI: %02x:%02x.%u  vendor=%04x device=%04x class=%02x:%02x irq=%u\n",
                  bus, slot, func,
                  d->vendor_id, d->device_id,
                  d->class_code, d->subclass,
                  d->irq_line);
}

void pci_init(void) {
    device_count = 0;
    serial_printf("PCI: enumerating...\n");

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            /* 先检查 func 0 */
            uint32_t id = pci_read32((uint8_t)bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            probe((uint8_t)bus, slot, 0);

            /* 如果是多功能设备，继续扫 func 1-7 */
            uint32_t hdr = pci_read32((uint8_t)bus, slot, 0, 0x0C);
            uint8_t header_type = (hdr >> 16) & 0xFF;
            if (header_type & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    uint32_t id2 = pci_read32((uint8_t)bus, slot, func, 0x00);
                    if ((id2 & 0xFFFF) == 0xFFFF) continue;
                    probe((uint8_t)bus, slot, func);
                }
            }
        }
    }
    serial_printf("PCI: %d devices found\n", device_count);
}

pci_device_t* pci_find(uint16_t vendor, uint16_t device) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor && devices[i].device_id == device) {
            return &devices[i];
        }
    }
    return 0;
}