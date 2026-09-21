#include "persist.h"
#include "ramfs.h"
#include "ata.h"
#include "heap.h"
#include "serial.h"
#include <stdint.h>

#define BLOB_MAX (PERSIST_MAX_SECTORS * ATA_SECTOR_SIZE)

int persist_load(void) {
    uint8_t* blob = (uint8_t*)kmalloc(BLOB_MAX);
    if (!blob) return -1;

    /* 只读第一个扇区，看 magic */
    if (ata_read_sectors(PERSIST_START_LBA, 1, blob) < 0) {
        kfree(blob);
        return -1;
    }

    const char* magic = "FLYOSFS1";
    int ok = 1;
    for (int i = 0; i < 8; i++) if (blob[i] != (uint8_t)magic[i]) ok = 0;

    if (!ok) {
        serial_printf("PERSIST: no filesystem on disk, starting empty\n");
        kfree(blob);
        return 0;
    }

    uint32_t total_size = 0;
    for (int i = 0; i < 4; i++) total_size |= (uint32_t)blob[12 + i] << (8 * i);
    if (total_size > BLOB_MAX) total_size = BLOB_MAX;

    uint32_t sectors = (total_size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    if (sectors < 1) sectors = 1;

    if (ata_read_sectors(PERSIST_START_LBA, (uint8_t)sectors, blob) < 0) {
        kfree(blob);
        return -1;
    }

    int r = ramfs_deserialize(blob, total_size);
    if (r < 0) serial_printf("PERSIST: deserialize failed\n");
    else serial_printf("PERSIST: loaded %lu bytes\n", total_size);

    kfree(blob);
    return r;
}

int persist_save(void) {
    uint8_t* blob = (uint8_t*)kmalloc(BLOB_MAX);
    if (!blob) return -1;

    uint64_t len = ramfs_serialize(blob, BLOB_MAX);
    if (len == 0) {
        kfree(blob);
        return -1;
    }

    uint32_t sectors = (uint32_t)((len + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE);
    if (sectors > PERSIST_MAX_SECTORS) sectors = PERSIST_MAX_SECTORS;

    /* ata_write_sectors 单次最大 255 扇区，分块写 */
    uint32_t lba = PERSIST_START_LBA;
    uint32_t remain = sectors;
    uint8_t* p = blob;
    while (remain > 0) {
        uint8_t chunk = remain > 128 ? 128 : (uint8_t)remain;
        if (ata_write_sectors(lba, chunk, p) < 0) {
            kfree(blob);
            return -1;
        }
        lba += chunk;
        p += (uint32_t)chunk * ATA_SECTOR_SIZE;
        remain -= chunk;
    }

    serial_printf("PERSIST: saved %lu bytes (%lu sectors)\n",
                  len, (uint64_t)sectors);
    kfree(blob);
    return 0;
}