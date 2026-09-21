#ifndef MYOS_RAMFS_H
#define MYOS_RAMFS_H

#include <stdint.h>

#define RAMFS_NAME_MAX 64

typedef enum {
    NODE_FILE = 0,
    NODE_DIR  = 1
} node_type_t;

typedef struct ramfs_node {
    char     name[RAMFS_NAME_MAX];
    node_type_t type;
    uint8_t* data;                 /* 仅文件 */
    uint64_t size;
    uint64_t capacity;
    struct ramfs_node* parent;
    struct ramfs_node* children;   /* 目录第一个孩子 */
    struct ramfs_node* next;       /* 同级下一个 */
} ramfs_node_t;

void          ramfs_init(void);
ramfs_node_t* ramfs_root(void);
ramfs_node_t* ramfs_cwd(void);
void          ramfs_set_cwd(ramfs_node_t* n);

ramfs_node_t* ramfs_find_child(ramfs_node_t* dir, const char* name);
ramfs_node_t* ramfs_lookup(const char* path);   /* 相对 cwd 或绝对路径 */

ramfs_node_t* ramfs_create_file(ramfs_node_t* dir, const char* name);
ramfs_node_t* ramfs_mkdir(ramfs_node_t* dir, const char* name);
int           ramfs_unlink_node(ramfs_node_t* node);

int ramfs_write(ramfs_node_t* f, const void* buf, uint64_t n, uint64_t off);
int ramfs_read (ramfs_node_t* f, void* buf, uint64_t n, uint64_t off);

/* 构造绝对路径，从根开始拼接 */
void ramfs_get_path(ramfs_node_t* n, char* out, int max);

/* 持久化 */
uint64_t ramfs_serialize  (uint8_t* out, uint64_t max);
int      ramfs_deserialize(const uint8_t* in, uint64_t len);

#endif