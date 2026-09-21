#include "ramfs.h"
#include "heap.h"
#include "serial.h"
#include <stdint.h>

static ramfs_file_t* files = 0;

static int str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void str_copy(char* dst, const char* src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

void ramfs_init(void) {
    files = 0;
    serial_printf("RAMFS: initialized\n");
}

ramfs_file_t* ramfs_find(const char* name) {
    for (ramfs_file_t* f = files; f; f = f->next) {
        if (str_eq(f->name, name)) return f;
    }
    return 0;
}

ramfs_file_t* ramfs_create(const char* name) {
    if (ramfs_find(name)) return 0;
    ramfs_file_t* f = (ramfs_file_t*)kzalloc(sizeof(ramfs_file_t));
    if (!f) return 0;
    str_copy(f->name, name, RAMFS_NAME_MAX);
    f->data = 0;
    f->size = 0;
    f->capacity = 0;
    f->next = files;
    files = f;
    return f;
}

int ramfs_unlink(const char* name) {
    ramfs_file_t** pp = &files;
    while (*pp) {
        if (str_eq((*pp)->name, name)) {
            ramfs_file_t* f = *pp;
            *pp = f->next;
            if (f->data) kfree(f->data);
            kfree(f);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

int ramfs_write(ramfs_file_t* f, const void* buf, uint64_t n, uint64_t off) {
    uint64_t need = off + n;
    if (need > f->capacity) {
        uint64_t new_cap = f->capacity ? f->capacity : 64;
        while (new_cap < need) new_cap *= 2;
        uint8_t* new_data = (uint8_t*)kmalloc(new_cap);
        if (!new_data) return -1;
        if (f->data) {
            for (uint64_t i = 0; i < f->size; i++) new_data[i] = f->data[i];
            kfree(f->data);
        }
        f->data = new_data;
        f->capacity = new_cap;
    }
    const uint8_t* src = (const uint8_t*)buf;
    for (uint64_t i = 0; i < n; i++) f->data[off + i] = src[i];
    if (off + n > f->size) f->size = off + n;
    return (int)n;
}

int ramfs_read(ramfs_file_t* f, void* buf, uint64_t n, uint64_t off) {
    if (off >= f->size) return 0;
    uint64_t avail = f->size - off;
    if (n > avail) n = avail;
    uint8_t* dst = (uint8_t*)buf;
    for (uint64_t i = 0; i < n; i++) dst[i] = f->data[off + i];
    return (int)n;
}

ramfs_file_t* ramfs_first(void) {
    return files;
}

/* ===== 序列化：把整个 ramfs 写成连续 blob ===== */
uint64_t ramfs_serialize(uint8_t* out, uint64_t max) {
    if (max < 16) return 0;

    uint64_t pos = 0;
    /* 头部：magic + file_count + total_size，先占位 */
    const char* magic = "FLYOSFS1";
    for (int i = 0; i < 8; i++) out[pos++] = (uint8_t)magic[i];
    pos += 8;   /* file_count + total_size 占位 */

    uint32_t count = 0;
    for (ramfs_file_t* f = files; f; f = f->next) {
        uint64_t nlen = 0;
        while (f->name[nlen]) nlen++;
        if (nlen > 255) nlen = 255;

        if (pos + 1 + nlen + 4 + f->size > max) break;

        out[pos++] = (uint8_t)nlen;
        for (uint64_t i = 0; i < nlen; i++) out[pos++] = (uint8_t)f->name[i];
        for (int i = 0; i < 4; i++) out[pos++] = (uint8_t)((f->size >> (8 * i)) & 0xFF);
        for (uint64_t i = 0; i < f->size; i++) out[pos++] = f->data[i];
        count++;
    }

    /* 回填 file_count / total_size */
    for (int i = 0; i < 4; i++) out[8  + i] = (uint8_t)((count    >> (8 * i)) & 0xFF);
    for (int i = 0; i < 4; i++) out[12 + i] = (uint8_t)((pos      >> (8 * i)) & 0xFF);
    return pos;
}

int ramfs_deserialize(const uint8_t* in, uint64_t len) {
    if (len < 16) return -1;
    const char* magic = "FLYOSFS1";
    for (int i = 0; i < 8; i++) {
        if (in[i] != (uint8_t)magic[i]) return -1;
    }

    uint32_t count = 0;
    for (int i = 0; i < 4; i++) count |= (uint32_t)in[8 + i] << (8 * i);

    uint64_t pos = 16;
    for (uint32_t k = 0; k < count; k++) {
        if (pos >= len) return -1;
        uint8_t nlen = in[pos++];
        if (pos + nlen + 4 > len) return -1;

        char name[RAMFS_NAME_MAX];
        uint64_t n_copy = nlen < RAMFS_NAME_MAX - 1 ? nlen : RAMFS_NAME_MAX - 1;
        for (uint64_t i = 0; i < n_copy; i++) name[i] = (char)in[pos + i];
        name[n_copy] = 0;
        pos += nlen;

        uint32_t size = 0;
        for (int i = 0; i < 4; i++) size |= (uint32_t)in[pos + i] << (8 * i);
        pos += 4;

        if (pos + size > len) return -1;

        ramfs_file_t* f = ramfs_create(name);
        if (!f) return -1;
        ramfs_write(f, in + pos, size, 0);
        pos += size;
    }
    return 0;
}