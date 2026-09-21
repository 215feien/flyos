#include "file.h"
#include "ramfs.h"
#include "serial.h"
#include <stdint.h>

#define MAX_FD 64

typedef struct {
    int used;
    ramfs_node_t* node;
    uint64_t pos;          /* 文件：读写位置；目录：已读子项数 */
    int is_dir;
} file_t;

static file_t fd_table[MAX_FD];

void file_init(void) {
    for (int i = 0; i < MAX_FD; i++) {
        fd_table[i].used = 0;
        fd_table[i].node = 0;
        fd_table[i].pos = 0;
        fd_table[i].is_dir = 0;
    }
}

int file_open(const char* name) {
    ramfs_node_t* n = ramfs_lookup(name);
    if (!n) {
        /* 不存在就在 cwd 下创建文件 */
        n = ramfs_create_file(ramfs_cwd(), name);
        if (!n) return -1;
    }
    if (n->type != NODE_FILE) return -1;

    for (int i = 3; i < MAX_FD; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].node = n;
            fd_table[i].pos = 0;
            fd_table[i].is_dir = 0;
            return i;
        }
    }
    return -1;
}

int file_close(int fd) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    fd_table[fd].used = 0;
    fd_table[fd].node = 0;
    fd_table[fd].pos = 0;
    fd_table[fd].is_dir = 0;
    return 0;
}

int64_t file_read(int fd, void* buf, uint64_t n) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    if (fd_table[fd].is_dir) return -1;
    int r = ramfs_read(fd_table[fd].node, buf, n, fd_table[fd].pos);
    if (r > 0) fd_table[fd].pos += (uint64_t)r;
    return r;
}

int64_t file_write(int fd, const void* buf, uint64_t n) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    if (fd_table[fd].is_dir) return -1;
    int r = ramfs_write(fd_table[fd].node, buf, n, fd_table[fd].pos);
    if (r > 0) fd_table[fd].pos += (uint64_t)r;
    return r;
}

int dir_open(const char* name) {
    ramfs_node_t* n = ramfs_lookup(name);
    if (!n || n->type != NODE_DIR) return -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].node = n;
            fd_table[i].pos = 0;
            fd_table[i].is_dir = 1;
            return i;
        }
    }
    return -1;
}

int64_t dir_read(int fd, char* name_out, int max, int* type_out) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    if (!fd_table[fd].is_dir) return -1;

    ramfs_node_t* dir = fd_table[fd].node;
    uint64_t target = fd_table[fd].pos;
    uint64_t idx = 0;
    for (ramfs_node_t* c = dir->children; c; c = c->next, idx++) {
        if (idx == target) {
            int i = 0;
            for (; i < max - 1 && c->name[i]; i++) name_out[i] = c->name[i];
            name_out[i] = 0;
            if (type_out) *type_out = (int)c->type;
            fd_table[fd].pos++;
            return 1;
        }
    }
    return 0;   /* 结束 */
}