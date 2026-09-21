#ifndef MYOS_RAMFS_H
#define MYOS_RAMFS_H

#include <stdint.h>

#define RAMFS_NAME_MAX 64

typedef struct ramfs_file {
    char     name[RAMFS_NAME_MAX];
    uint8_t* data;
    uint64_t size;
    uint64_t capacity;
    struct ramfs_file* next;
} ramfs_file_t;

void          ramfs_init(void);
ramfs_file_t* ramfs_find(const char* name);
ramfs_file_t* ramfs_create(const char* name);
int           ramfs_unlink(const char* name);
int           ramfs_write(ramfs_file_t* f, const void* buf, uint64_t n, uint64_t off);
int           ramfs_read (ramfs_file_t* f, void* buf, uint64_t n, uint64_t off);
ramfs_file_t* ramfs_first(void);

/* 新增：序列化 / 反序列化 */
uint64_t ramfs_serialize  (uint8_t* out, uint64_t max);
int      ramfs_deserialize(const uint8_t* in, uint64_t len);

#endif