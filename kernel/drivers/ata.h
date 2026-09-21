#ifndef MYOS_ATA_H
#define MYOS_ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

int ata_init(void);
int ata_read_sectors (uint32_t lba, uint8_t count, void* buf);
int ata_write_sectors(uint32_t lba, uint8_t count, const void* buf);

uint32_t ata_disk_sectors(void);
const char* ata_disk_model(void);

#endif