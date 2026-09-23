#include "fat16.h"
#include "ata.h"
#include "heap.h"
#include "serial.h"
#include <stdint.h>

#define SECTOR_SIZE 512

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed)) fat16_bpb_t;

static uint32_t part_lba = 0;
static uint16_t bytes_per_sector = 512;
static uint8_t  sectors_per_cluster = 1;
static uint16_t reserved_sectors = 1;
static uint8_t  num_fats = 2;
static uint16_t root_entries = 512;
static uint16_t fat_size = 0;
static uint32_t fat_start_lba = 0;
static uint32_t root_start_lba = 0;
static uint32_t data_start_lba = 0;

static int read_sector(uint32_t lba, void* buf) {
    return ata_read_sectors(part_lba + lba, 1, buf);
}

void fat16_init(uint32_t partition_lba) {
    part_lba = partition_lba;

    uint8_t sector[512];
    if (read_sector(0, sector) < 0) {
        serial_printf("FAT16: read failed\n");
        return;
    }

    fat16_bpb_t* bpb = (fat16_bpb_t*)sector;
    bytes_per_sector   = bpb->bytes_per_sector;
    sectors_per_cluster = bpb->sectors_per_cluster;
    reserved_sectors   = bpb->reserved_sectors;
    num_fats           = bpb->num_fats;
    root_entries       = bpb->root_entries;
    fat_size           = bpb->fat_size_16;

    fat_start_lba = reserved_sectors;
    root_start_lba = reserved_sectors + num_fats * fat_size;
    uint32_t root_sectors = (root_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    data_start_lba = root_start_lba + root_sectors;

    serial_printf("FAT16: bps=%u spc=%u reserved=%u fats=%u root_entries=%u fat_size=%u\n",
                  bytes_per_sector, sectors_per_cluster, reserved_sectors,
                  num_fats, root_entries, fat_size);
    serial_printf("FAT16: fat_start=%lu root_start=%lu data_start=%lu\n",
                  (uint64_t)fat_start_lba, (uint64_t)root_start_lba,
                  (uint64_t)data_start_lba);
}

/* 8.3 名转可读字符串，如 "A       TXT" -> "A.TXT" */
static void dirent_name(const fat16_dirent_t* d, char* out) {
    int i = 0;
    for (int j = 0; j < 8; j++) {
        if (d->name[j] == ' ') break;
        out[i++] = d->name[j];
    }
    if (d->ext[0] != ' ') {
        out[i++] = '.';
        for (int j = 0; j < 3; j++) {
            if (d->ext[j] == ' ') break;
            out[i++] = d->ext[j];
        }
    }
    out[i] = 0;
}

int fat16_list_root(char* buf, int max) {
    uint32_t root_sectors = (root_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return -1;

    int pos = 0;
    int count = 0;
    for (uint32_t s = 0; s < root_sectors; s++) {
        if (read_sector(root_start_lba + s, sector) < 0) break;
        fat16_dirent_t* ents = (fat16_dirent_t*)sector;
        for (int i = 0; i < (int)(512 / 32); i++) {
            fat16_dirent_t* d = &ents[i];
            if (d->name[0] == 0) { kfree(sector); return pos; }   /* 结尾 */
            if ((uint8_t)d->name[0] == 0xE5) continue;              /* 已删除 */
            if (d->attr == 0x0F) continue;                          /* LFN */

            char name[32];
            dirent_name(d, name);
            int len = 0;
            while (name[len]) len++;
            if (pos + len + 2 > max) break;
            for (int j = 0; j < len; j++) buf[pos++] = name[j];
            if (d->attr & 0x10) buf[pos++] = '/';
            buf[pos++] = '\n';
            count++;
        }
    }
    kfree(sector);
    return pos;
}

static void dirent_raw(const fat16_dirent_t* d, uint8_t* out11) {
    for (int i = 0; i < 8; i++) out11[i]     = (uint8_t)d->name[i];
    for (int i = 0; i < 3; i++) out11[8 + i] = (uint8_t)d->ext[i];
}

static fat16_dirent_t* find_root_entry(const char* name) {
    uint32_t root_sectors = (root_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    uint8_t* sector = (uint8_t*)kmalloc(512);
    if (!sector) return 0;

    /* 目标 11 字节，空格填充 */
    uint8_t want[11];
    for (int i = 0; i < 11; i++) want[i] = 0x20;

    int i = 0;
    for (; name[i] && name[i] != '.' && i < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        want[i] = (uint8_t)c;
    }
    if (name[i] == '.') {
        i++;
        for (int j = 8; name[i] && j < 11; i++, j++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            want[j] = (uint8_t)c;
        }
    }

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (read_sector(root_start_lba + s, sector) < 0) break;
        fat16_dirent_t* ents = (fat16_dirent_t*)sector;
        for (int k = 0; k < (int)(512 / 32); k++) {
            fat16_dirent_t* d = &ents[k];
            if (d->name[0] == 0) { kfree(sector); return 0; }
            if ((uint8_t)d->name[0] == 0xE5) continue;
            if (d->attr == 0x0F) continue;

            uint8_t raw[11];
            dirent_raw(d, raw);

            int match = 1;
            for (int m = 0; m < 11; m++) {
                if (raw[m] != want[m]) { match = 0; break; }
            }
            if (match) {
                fat16_dirent_t* ret = (fat16_dirent_t*)kmalloc(32);
                if (ret) *ret = *d;
                kfree(sector);
                return ret;
            }
        }
    }
    kfree(sector);
    return 0;
}

uint64_t fat16_file_size(const char* name) {
    fat16_dirent_t* d = find_root_entry(name);
    if (!d) return 0;
    uint32_t sz = d->size;
    kfree(d);
    return sz;
}

/* 读 FAT 表项 */
static uint16_t fat_next(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t sector = fat_start_lba + fat_offset / 512;
    uint32_t off = fat_offset % 512;
    uint8_t buf[512];
    if (read_sector(sector, buf) < 0) return 0xFFFF;
    return *(uint16_t*)(buf + off);
}

int fat16_read_file(const char* name, void* out, int max) {
    fat16_dirent_t* d = find_root_entry(name);
    if (!d) return -1;

    uint32_t size = d->size;
    uint16_t cluster = d->cluster_lo;
    kfree(d);

    if (size > (uint32_t)max) size = max;
    if (size == 0 || cluster < 2) return 0;

    uint8_t* dst = (uint8_t*)out;
    uint32_t done = 0;
    uint8_t buf[512];

    while (cluster >= 2 && cluster < 0xFFF8 && done < size) {
        uint32_t lba = data_start_lba + (cluster - 2) * sectors_per_cluster;
        for (int s = 0; s < sectors_per_cluster && done < size; s++) {
            if (read_sector(lba + s, buf) < 0) return -1;
            for (int i = 0; i < 512 && done < size; i++) {
                dst[done++] = buf[i];
            }
        }
        cluster = fat_next(cluster);
    }
    return (int)done;
}