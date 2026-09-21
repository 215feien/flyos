#ifndef MYOS_FB_H
#define MYOS_FB_H

#include <stdint.h>

typedef struct {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
    uint8_t  r_pos, r_size;
    uint8_t  g_pos, g_size;
    uint8_t  b_pos, b_size;
} fb_info_t;

void       fb_init(uint32_t mb_info_phys);
fb_info_t* fb_get_info(void);

uint32_t   fb_rgb(uint8_t r, uint8_t g, uint8_t b);
void       fb_putpixel(int x, int y, uint32_t color);
void       fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void       fb_clear(uint32_t color);

#endif