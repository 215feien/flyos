#include "fb.h"
#include "vmm.h"
#include "pmm.h"
#include "serial.h"
#include <stdint.h>

#define PAGE_SIZE_ 4096

static fb_info_t fbi;

struct mb_tag {
    uint32_t type;
    uint32_t size;
};

void fb_init(uint32_t mb_info_phys) {
    uint8_t* mb = (uint8_t*)(uintptr_t)mb_info_phys;
    uint32_t total = *(uint32_t*)mb;
    uint8_t* p = mb + 8;
    uint8_t* end = mb + total;

    while (p < end) {
        struct mb_tag* tag = (struct mb_tag*)p;
        if (tag->type == 0) break;
        if (tag->type == 8) {
            uint8_t* d = (uint8_t*)tag + 8;
            fbi.addr   = *(uint64_t*)d; d += 8;
            fbi.pitch  = *(uint32_t*)d; d += 4;
            fbi.width  = *(uint32_t*)d; d += 4;
            fbi.height = *(uint32_t*)d; d += 4;
            fbi.bpp    = *(uint8_t*)d;  d += 1;
            uint8_t fb_type = *(uint8_t*)d; d += 1;
            d += 2;   /* reserved */

            if (fb_type == 1) {
                fbi.r_pos = *(uint8_t*)d; d += 1;
                fbi.r_size = *(uint8_t*)d; d += 1;
                fbi.g_pos = *(uint8_t*)d; d += 1;
                fbi.g_size = *(uint8_t*)d; d += 1;
                fbi.b_pos = *(uint8_t*)d; d += 1;
                fbi.b_size = *(uint8_t*)d; d += 1;
            }
        }
        p += (tag->size + 7) & ~7U;
    }

    if (!fbi.addr) {
        serial_printf("FB: no framebuffer from GRUB\n");
        return;
    }

    serial_printf("FB: %ux%u bpp=%u pitch=%u addr=0x%lx\n",
                  fbi.width, fbi.height, fbi.bpp, fbi.pitch, fbi.addr);

    /* 把 framebuffer 物理区域映射到相同虚拟地址（恒等映射） */
    uint64_t fb_size = (uint64_t)fbi.pitch * fbi.height;
    uint64_t first = fbi.addr & ~0xFFFULL;
    uint64_t last  = (fbi.addr + fb_size + 0xFFF) & ~0xFFFULL;

    for (uint64_t va = first; va < last; va += PAGE_SIZE_) {
        vmm_map(va, va, VMM_WRITABLE);
    }

    serial_printf("FB: mapped %lu bytes\n", last - first);
}

fb_info_t* fb_get_info(void) { return &fbi; }

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t v = 0;
    v |= ((uint32_t)r >> (8 - fbi.r_size)) << fbi.r_pos;
    v |= ((uint32_t)g >> (8 - fbi.g_size)) << fbi.g_pos;
    v |= ((uint32_t)b >> (8 - fbi.b_size)) << fbi.b_pos;
    return v;
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= fbi.width || (uint32_t)y >= fbi.height) return;
    uint8_t* base = (uint8_t*)(uintptr_t)fbi.addr;
    uint32_t* pixel = (uint32_t*)(base + (uint64_t)y * fbi.pitch + (uint64_t)x * (fbi.bpp / 8));
    *pixel = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_putpixel(x + i, y + j, color);
        }
    }
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, (int)fbi.width, (int)fbi.height, color);
}