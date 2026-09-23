#ifndef MYOS_FAT16_H
#define MYOS_FAT16_H

#include <stdint.h>

/* FAT16 目录项（8.3 格式，32 字节） */
typedef struct {
    char     name[8];        /* 8.3 名 */
    char     ext[3];
    uint8_t  attr;           /* 0x10 = 目录 */
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_hi;     /* FAT16 不用 */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_lo;     /* 起始簇号 */
    uint32_t size;           /* 文件大小 */
} __attribute__((packed)) fat16_dirent_t;

void     fat16_init(uint32_t partition_lba);
int      fat16_list_root(char* buf, int max);
int      fat16_read_file(const char* name, void* buf, int max);
uint64_t fat16_file_size(const char* name);

#endif