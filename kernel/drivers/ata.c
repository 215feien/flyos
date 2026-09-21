#include "ata.h"
#include "serial.h"
#include <stdint.h>

/* 主 IDE 通道 PIO 端口 */
#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_FEATURES  0x1F1
#define ATA_SECCOUNT  0x1F2
#define ATA_LBA_LO    0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HI    0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_STATUS    0x1F7
#define ATA_COMMAND   0x1F7
#define ATA_CTRL      0x3F6

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH    0xE7

#define STATUS_BSY 0x80
#define STATUS_RDY 0x40
#define STATUS_DRQ 0x08
#define STATUS_ERR 0x01
#define STATUS_DF  0x20

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    /* 读 0x3F6 状态寄存器，产生 ~400ns 延迟 */
    for (int i = 0; i < 4; i++) (void)inb(ATA_CTRL);
}

static uint32_t disk_sectors = 0;
static char disk_model[41] = {0};

/* 等待 BSY 清除 */
static int wait_not_busy(void) {
    for (int i = 0; i < 1000000; i++) {
        if ((inb(ATA_STATUS) & STATUS_BSY) == 0) return 0;
    }
    return -1;
}

/* 等待 DRQ 置位（数据可读写） */
static int wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & STATUS_ERR) return -1;
        if (s & STATUS_DF)  return -1;
        if ((s & STATUS_BSY) == 0 && (s & STATUS_DRQ)) return 0;
    }
    return -1;
}

int ata_init(void) {
    /* 选择主盘 */
    outb(ATA_DRIVE, 0xA0);
    io_wait();

    /* 关中断，清 SECCOUNT/LBA */
    outb(ATA_CTRL, 0x02);
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);

    /* IDENTIFY */
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_STATUS);
    if (status == 0) {
        serial_printf("ATA: no drive on primary master\n");
        return -1;
    }

    if (wait_not_busy() < 0) {
        serial_printf("ATA: busy timeout\n");
        return -1;
    }

    /* 检查 LBA_MID/LBA_HI：如果是 0，说明是 ATA 设备；非 0 是 ATAPI */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) {
        serial_printf("ATA: not an ATA drive (ATAPI?)\n");
        return -1;
    }

    if (wait_drq() < 0) {
        serial_printf("ATA: identify failed\n");
        return -1;
    }

    /* 读 256 个 16-bit 字 */
    uint16_t ident[256];
    for (int i = 0; i < 256; i++) {
        ident[i] = inw(ATA_DATA);
    }

    /* 型号字符串：word 27-46，按 big-endian 排列 */
    for (int i = 0; i < 20; i++) {
        disk_model[i * 2]     = (char)(ident[27 + i] >> 8);
        disk_model[i * 2 + 1] = (char)(ident[27 + i] & 0xFF);
    }
    disk_model[40] = 0;

    /* LBA28 扇区数：word 60-61 */
    disk_sectors = (uint32_t)ident[60] | ((uint32_t)ident[61] << 16);

    /* 去掉型号尾部空格 */
    for (int i = 39; i >= 0 && disk_model[i] == ' '; i--) disk_model[i] = 0;

    serial_printf("ATA: model='%s' sectors=%lu (%lu MB)\n",
                  disk_model, (uint64_t)disk_sectors,
                  (uint64_t)disk_sectors / 2048);
    return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buf) {
    if (!disk_sectors) return -1;

    if (wait_not_busy() < 0) return -1;

    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));   /* LBA 模式，主盘 */
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ);

    uint16_t* ptr = (uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (wait_drq() < 0) return -1;
        for (int i = 0; i < 256; i++) {
            ptr[i] = inw(ATA_DATA);
        }
        ptr += 256;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void* buf) {
    if (!disk_sectors) return -1;

    if (wait_not_busy() < 0) return -1;

    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    const uint16_t* ptr = (const uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (wait_drq() < 0) return -1;
        for (int i = 0; i < 256; i++) {
            outw(ATA_DATA, ptr[i]);
        }
        ptr += 256;
        io_wait();
    }

    /* 刷新缓存到磁盘 */
    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    if (wait_not_busy() < 0) return -1;

    return 0;
}

uint32_t ata_disk_sectors(void) { return disk_sectors; }
const char* ata_disk_model(void) { return disk_model; }