#include "persist.h"
#include "ramfs.h"
#include "ata.h"
#include "heap.h"
#include "serial.h"
#include <stdint.h>

#define BLOB_MAX (PERSIST_MAX_SECTORS * ATA_SECTOR_SIZE)

int persist_load(void) {
    uint8_t* blob = (uint8_t*)kmalloc(BLOB_MAX);
    if (!blob) {
        serial_printf("PERSIST: OOM for load\n");
        return -1;
    }

    /* 先读 1 个扇区，检查 magic 和 total_size */
    if (ata_read_sectors(PERSIST_START_LBA, 1, blob) < 0) {
        kfree(blob);
        return -1;
    }

    const char* magic = "FLYOSFS2";
    int ok = 1;
    for (int i = 0; i < 8; i++) if (blob[i] != (uint8_t)magic[i]) ok = 0;

    if (!ok) {
        serial_printf("PERSIST: no v2 fs on disk, starting empty\n");
        kfree(blob);
        return 0;
    }

    /* total_size 在偏移 8-11（与 ramfs_serialize 一致） */
    uint32_t total_size = 0;
    for (int i = 0; i < 4; i++) total_size |= (uint32_t)blob[8 + i] << (8 * i);
    if (total_size > BLOB_MAX) total_size = BLOB_MAX;
    if (total_size < 16) total_size = 16;

    uint32_t sectors = (total_size + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    if (sectors < 1) sectors = 1;

    /* ata_read_sectors 单次最多 255 扇区，分块读 */
    uint32_t lba = PERSIST_START_LBA;
    uint32_t remain = sectors;
    uint8_t* p = blob;
    while (remain > 0) {
        uint8_t chunk = remain > 128 ? 128 : (uint8_t)remain;
        if (ata_read_sectors(lba, chunk, p) < 0) {
            kfree(blob);
            return -1;
        }
        lba += chunk;
        p += (uint32_t)chunk * ATA_SECTOR_SIZE;
        remain -= chunk;
    }

    int r = ramfs_deserialize(blob, total_size);
    if (r < 0) serial_printf("PERSIST: deserialize failed\n");
    else serial_printf("PERSIST: loaded %lu bytes\n", total_size);

    kfree(blob);
    return r;
}

int persist_save(void) {
    uint8_t* blob = (uint8_t*)kmalloc(BLOB_MAX);
    if (!blob) {
        serial_printf("PERSIST: OOM for save\n");
        return -1;
    }

    uint64_t len = ramfs_serialize(blob, BLOB_MAX);
    if (len == 0) {
        kfree(blob);
        return -1;
    }

    uint32_t sectors = (uint32_t)((len + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE);
    if (sectors > PERSIST_MAX_SECTORS) sectors = PERSIST_MAX_SECTORS;

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