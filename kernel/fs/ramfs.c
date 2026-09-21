#include "ramfs.h"
#include "heap.h"
#include "serial.h"
#include <stdint.h>

static ramfs_node_t* root = 0;
static ramfs_node_t* cwd  = 0;

static int str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void str_copy(char* dst, const char* src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static ramfs_node_t* node_alloc(node_type_t type, const char* name, ramfs_node_t* parent) {
    ramfs_node_t* n = (ramfs_node_t*)kzalloc(sizeof(ramfs_node_t));
    if (!n) return 0;
    str_copy(n->name, name, RAMFS_NAME_MAX);
    n->type = type;
    n->parent = parent;
    n->children = 0;
    n->next = 0;
    n->data = 0;
    n->size = 0;
    n->capacity = 0;
    return n;
}

void ramfs_init(void) {
    root = node_alloc(NODE_DIR, "/", 0);
    cwd  = root;
    serial_printf("RAMFS: initialized (root)\n");
}

ramfs_node_t* ramfs_root(void) { return root; }
ramfs_node_t* ramfs_cwd(void)  { return cwd; }
void ramfs_set_cwd(ramfs_node_t* n) { if (n && n->type == NODE_DIR) cwd = n; }

ramfs_node_t* ramfs_find_child(ramfs_node_t* dir, const char* name) {
    if (!dir || dir->type != NODE_DIR) return 0;
    for (ramfs_node_t* c = dir->children; c; c = c->next) {
        if (str_eq(c->name, name)) return c;
    }
    return 0;
}

ramfs_node_t* ramfs_lookup(const char* path) {
    if (!path || !*path) return 0;

    ramfs_node_t* cur;
    const char* p;
    if (*path == '/') { cur = root; p = path + 1; }
    else              { cur = cwd;  p = path;     }

    char name[RAMFS_NAME_MAX];
    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < RAMFS_NAME_MAX - 1) name[i++] = *p++;
        name[i] = 0;
        while (*p == '/') p++;

        if (i == 0) continue;
        if (name[0] == '.' && name[1] == 0) continue;
        if (name[0] == '.' && name[1] == '.' && name[2] == 0) {
            if (cur->parent) cur = cur->parent;
            continue;
        }
        if (cur->type != NODE_DIR) return 0;
        ramfs_node_t* child = ramfs_find_child(cur, name);
        if (!child) return 0;
        cur = child;
    }
    return cur;
}

ramfs_node_t* ramfs_create_file(ramfs_node_t* dir, const char* name) {
    if (!dir || dir->type != NODE_DIR) return 0;
    if (ramfs_find_child(dir, name)) return 0;
    ramfs_node_t* n = node_alloc(NODE_FILE, name, dir);
    if (!n) return 0;
    n->next = dir->children;
    dir->children = n;
    return n;
}

ramfs_node_t* ramfs_mkdir(ramfs_node_t* dir, const char* name) {
    if (!dir || dir->type != NODE_DIR) return 0;
    if (ramfs_find_child(dir, name)) return 0;
    ramfs_node_t* n = node_alloc(NODE_DIR, name, dir);
    if (!n) return 0;
    n->next = dir->children;
    dir->children = n;
    return n;
}

int ramfs_unlink_node(ramfs_node_t* node) {
    if (!node || !node->parent) return -1;
    if (node->type == NODE_DIR && node->children) return -1;

    ramfs_node_t** pp = &node->parent->children;
    while (*pp && *pp != node) pp = &(*pp)->next;
    if (!*pp) return -1;
    *pp = node->next;

    if (node->data) kfree(node->data);
    kfree(node);
    return 0;
}

int ramfs_write(ramfs_node_t* f, const void* buf, uint64_t n, uint64_t off) {
    if (!f || f->type != NODE_FILE) return -1;
    uint64_t need = off + n;
    if (need > f->capacity) {
        uint64_t new_cap = f->capacity ? f->capacity : 64;
        while (new_cap < need) new_cap *= 2;
        uint8_t* nd = (uint8_t*)kmalloc(new_cap);
        if (!nd) return -1;
        if (f->data) {
            for (uint64_t i = 0; i < f->size; i++) nd[i] = f->data[i];
            kfree(f->data);
        }
        f->data = nd;
        f->capacity = new_cap;
    }
    const uint8_t* src = (const uint8_t*)buf;
    for (uint64_t i = 0; i < n; i++) f->data[off + i] = src[i];
    if (need > f->size) f->size = need;
    return (int)n;
}

int ramfs_read(ramfs_node_t* f, void* buf, uint64_t n, uint64_t off) {
    if (!f || f->type != NODE_FILE) return -1;
    if (off >= f->size) return 0;
    uint64_t avail = f->size - off;
    if (n > avail) n = avail;
    uint8_t* dst = (uint8_t*)buf;
    for (uint64_t i = 0; i < n; i++) dst[i] = f->data[off + i];
    return (int)n;
}

void ramfs_get_path(ramfs_node_t* n, char* out, int max) {
    if (!n || n == root) { str_copy(out, "/", max); return; }

    char parent_path[256];
    ramfs_get_path(n->parent, parent_path, sizeof(parent_path));

    int len = 0;
    while (parent_path[len]) len++;
    int i = 0;
    for (; i < len && i < max - 1; i++) out[i] = parent_path[i];
    if (i < max - 1 && parent_path[len - 1] != '/') out[i++] = '/';
    for (int j = 0; n->name[j] && i < max - 1; j++) out[i++] = n->name[j];
    out[i] = 0;
}

/* ===== 序列化 v2 ===== */

static uint64_t ser_node(ramfs_node_t* n, uint8_t* out, uint64_t pos, uint64_t max) {
    uint64_t nlen = 0;
    while (n->name[nlen]) nlen++;
    if (nlen > 255) nlen = 255;
    if (pos + 2 + nlen + 4 > max) return 0;

    out[pos++] = (uint8_t)n->type;
    out[pos++] = (uint8_t)nlen;
    for (uint64_t i = 0; i < nlen; i++) out[pos++] = (uint8_t)n->name[i];

    if (n->type == NODE_FILE) {
        for (int i = 0; i < 4; i++) out[pos++] = (uint8_t)((n->size >> (8 * i)) & 0xFF);
        if (pos + n->size > max) return 0;
        for (uint64_t i = 0; i < n->size; i++) out[pos++] = n->data[i];
    } else {
        for (ramfs_node_t* c = n->children; c; c = c->next) {
            pos = ser_node(c, out, pos, max);
            if (pos == 0) return 0;
        }
        if (pos + 2 > max) return 0;
        out[pos++] = 0xFF;
        out[pos++] = 0;
    }
    return pos;
}

uint64_t ramfs_serialize(uint8_t* out, uint64_t max) {
    if (max < 16) return 0;

    uint64_t pos = 0;
    const char* magic = "FLYOSFS2";
    for (int i = 0; i < 8; i++) out[pos++] = (uint8_t)magic[i];
    pos += 4;   /* total_size 占位：偏移 8-11 */

    /* 根目录的子节点 */
    for (ramfs_node_t* c = root->children; c; c = c->next) {
        pos = ser_node(c, out, pos, max);
        if (pos == 0) return 0;
    }

    for (int i = 0; i < 4; i++) out[8 + i] = (uint8_t)((pos >> (8 * i)) & 0xFF);
    return pos;
}

/* ===== 反序列化 v2 ===== */

static uint64_t deser_node(ramfs_node_t* parent, const uint8_t* in, uint64_t pos, uint64_t len) {
    if (pos + 2 > len) return 0;
    uint8_t type = in[pos++];
    uint8_t nlen = in[pos++];

    if (type == 0xFF) return pos;

    if (pos + nlen > len) return 0;
    char name[RAMFS_NAME_MAX];
    uint64_t nc = nlen < RAMFS_NAME_MAX - 1 ? nlen : RAMFS_NAME_MAX - 1;
    for (uint64_t i = 0; i < nc; i++) name[i] = (char)in[pos + i];
    name[nc] = 0;
    pos += nlen;

    if (type == NODE_FILE) {
        if (pos + 4 > len) return 0;
        uint32_t sz = 0;
        for (int i = 0; i < 4; i++) sz |= (uint32_t)in[pos + i] << (8 * i);
        pos += 4;
        if (pos + sz > len) return 0;
        ramfs_node_t* f = ramfs_create_file(parent, name);
        if (!f) return 0;
        ramfs_write(f, in + pos, sz, 0);
        return pos + sz;
    } else if (type == NODE_DIR) {
        ramfs_node_t* d = ramfs_mkdir(parent, name);
        if (!d) return 0;
        while (1) {
            if (pos + 2 > len) return 0;
            if (in[pos] == 0xFF && in[pos + 1] == 0) return pos + 2;
            uint64_t next = deser_node(d, in, pos, len);
            if (next == 0 || next <= pos) return 0;
            pos = next;
        }
    }
    return 0;
}

int ramfs_deserialize(const uint8_t* in, uint64_t len) {
    if (len < 16) return -1;
    const char* magic = "FLYOSFS2";
    for (int i = 0; i < 8; i++) if (in[i] != (uint8_t)magic[i]) return -1;

    uint32_t total = 0;
    for (int i = 0; i < 4; i++) total |= (uint32_t)in[8 + i] << (8 * i);
    if (total > len) total = len;

    uint64_t pos = 12;
    while (pos + 2 <= total) {
        if (in[pos] == 0xFF && in[pos + 1] == 0) break;
        uint64_t next = deser_node(root, in, pos, total);
        if (next == 0 || next <= pos) return -1;
        pos = next;
    }
    return 0;
}
