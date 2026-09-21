#include "fb_cursor.h"
#include "fb.h"

#define CUR_W 12
#define CUR_H 19

/* 0=透明, 1=黑边, 2=白填充 */
static const uint8_t cursor_bmp[CUR_H][CUR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,1,1,1,1,1,0},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,1,2,2,1,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0,0},
    {1,1,0,0,0,1,2,2,1,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};

static int cur_x = 640, cur_y = 400;
static int visible = 0;
static uint32_t saved[CUR_H][CUR_W];

static void save_under(void) {
    fb_info_t* fbi = fb_get_info();
    uint8_t* base = (uint8_t*)(uintptr_t)fbi->addr;
    uint32_t pitch = fbi->pitch;
    int bpp = fbi->bpp / 8;
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            int x = cur_x + i;
            int y = cur_y + j;
            if (x < 0 || y < 0 || (uint32_t)x >= fbi->width || (uint32_t)y >= fbi->height) {
                saved[j][i] = 0;
                continue;
            }
            uint32_t* p = (uint32_t*)(base + (uint64_t)y * pitch + (uint64_t)x * bpp);
            saved[j][i] = *p;
        }
    }
}

static void restore_under(void) {
    fb_info_t* fbi = fb_get_info();
    uint8_t* base = (uint8_t*)(uintptr_t)fbi->addr;
    uint32_t pitch = fbi->pitch;
    int bpp = fbi->bpp / 8;
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            int x = cur_x + i;
            int y = cur_y + j;
            if (x < 0 || y < 0 || (uint32_t)x >= fbi->width || (uint32_t)y >= fbi->height) continue;
            uint32_t* p = (uint32_t*)(base + (uint64_t)y * pitch + (uint64_t)x * bpp);
            *p = saved[j][i];
        }
    }
}

static void draw_cursor(void) {
    fb_info_t* fbi = fb_get_info();
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            int x = cur_x + i;
            int y = cur_y + j;
            if (x < 0 || y < 0 || (uint32_t)x >= fbi->width || (uint32_t)y >= fbi->height) continue;
            uint8_t v = cursor_bmp[j][i];
            if (v == 0) continue;
            uint32_t color = (v == 1) ? fb_rgb(0, 0, 0) : fb_rgb(255, 255, 255);
            fb_putpixel(x, y, color);
        }
    }
    visible = 1;
}

void fb_cursor_init(void) {
    fb_info_t* fbi = fb_get_info();
    cur_x = (int)fbi->width / 2;
    cur_y = (int)fbi->height / 2;
    save_under();
    draw_cursor();
}

void fb_cursor_move(int dx, int dy) {
    if (visible) {
        restore_under();
        visible = 0;
    }
    cur_x += dx;
    cur_y += dy;

    fb_info_t* fbi = fb_get_info();
    if (cur_x < 0) cur_x = 0;
    if (cur_y < 0) cur_y = 0;
    if (cur_x > (int)fbi->width  - CUR_W) cur_x = (int)fbi->width  - CUR_W;
    if (cur_y > (int)fbi->height - CUR_H) cur_y = (int)fbi->height - CUR_H;

    save_under();
    draw_cursor();
}

void fb_cursor_hide(void) {
    if (visible) {
        restore_under();
        visible = 0;
    }
}

void fb_cursor_show(void) {
    if (!visible) {
        save_under();
        draw_cursor();
    }
}

void fb_cursor_get_pos(int* x, int* y) {
    *x = cur_x;
    *y = cur_y;
}