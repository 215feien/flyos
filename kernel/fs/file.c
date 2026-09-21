#include "file.h"
#include "ramfs.h"
#include "serial.h"
#include <stdint.h>

#define MAX_FD 64

typedef struct {
    int used;
    ramfs_file_t* node;
    uint64_t pos;
} file_t;

static file_t fd_table[MAX_FD];

void file_init(void) {
    for (int i = 0; i < MAX_FD; i++) {
        fd_table[i].used = 0;
        fd_table[i].node = 0;
        fd_table[i].pos = 0;
    }
}

int file_open(const char* name) {
    /* fd 0,1,2 保留 */
    for (int i = 3; i < MAX_FD; i++) {
        if (!fd_table[i].used) {
            ramfs_file_t* f = ramfs_find(name);
            if (!f) f = ramfs_create(name);
            if (!f) return -1;
            fd_table[i].used = 1;
            fd_table[i].node = f;
            fd_table[i].pos = 0;
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
    return 0;
}

int64_t file_read(int fd, void* buf, uint64_t n) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    int r = ramfs_read(fd_table[fd].node, buf, n, fd_table[fd].pos);
    if (r > 0) fd_table[fd].pos += (uint64_t)r;
    return r;
}

int64_t file_write(int fd, const void* buf, uint64_t n) {
    if (fd < 3 || fd >= MAX_FD || !fd_table[fd].used) return -1;
    int r = ramfs_write(fd_table[fd].node, buf, n, fd_table[fd].pos);
    if (r > 0) fd_table[fd].pos += (uint64_t)r;
    return r;
}