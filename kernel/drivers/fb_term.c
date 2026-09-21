#include "fb_term.h"
#include "fb.h"
#include "font8x16.h"
#include <stdint.h>

#define CHAR_W 8
#define CHAR_H 16

#define MAX_COLS 160
#define MAX_ROWS 64

static int region_x = 0, region_y = 0;
static int region_w = 0, region_h = 0;
static int cols = 0, rows = 0;

static int cursor_x = 0, cursor_y = 0;
static uint32_t fg_color;
static uint32_t bg_color;

/* 字符缓冲：'-' 表示空格。内容只在 region 内。 */
static char screen_buf[MAX_ROWS][MAX_COLS];

static void draw_char_at(char c, int col, int row) {
    const uint8_t* glyph = font8x16[(uint8_t)c];
    int px = region_x + col * CHAR_W;
    int py = region_y + row * CHAR_H;
    for (int r = 0; r < CHAR_H; r++) {
        uint8_t bits = glyph[r];
        for (int cc = 0; cc < CHAR_W; cc++) {
            uint32_t color = (bits & (0x80 >> cc)) ? fg_color : bg_color;
            fb_putpixel(px + cc, py + r, color);
        }
    }
}

void fb_term_redraw(void) {
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            draw_char_at(screen_buf[r][c], c, r);
        }
    }
}

void fb_term_set_region(int x, int y, int w, int h) {
    region_x = x;
    region_y = y;
    region_w = w;
    region_h = h;
    cols = w / CHAR_W;
    rows = h / CHAR_H;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;

    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            screen_buf[r][c] = ' ';

    cursor_x = 0;
    cursor_y = 0;
}

void fb_term_clear(void) {
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            screen_buf[r][c] = ' ';
    cursor_x = 0;
    cursor_y = 0;
    fb_fill_rect(region_x, region_y, region_w, region_h, bg_color);
}

void fb_term_set_color(uint32_t fg, uint32_t bg) {
    fg_color = fg;
    bg_color = bg;
}

void fb_term_init(void) {
    fb_info_t* fbi = fb_get_info();
    int screen_cols = (int)(fbi->width / CHAR_W);
    int screen_rows = (int)(fbi->height / CHAR_H);
    if (screen_cols > MAX_COLS) screen_cols = MAX_COLS;
    if (screen_rows > MAX_ROWS) screen_rows = MAX_ROWS;

    fg_color = fb_rgb(220, 220, 220);
    bg_color = fb_rgb(10, 10, 40);

    region_x = 0;
    region_y = 0;
    region_w = (int)fbi->width;
    region_h = (int)fbi->height;
    cols = screen_cols;
    rows = screen_rows;

    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < MAX_COLS; c++)
            screen_buf[r][c] = ' ';

    cursor_x = 0;
    cursor_y = 0;
}

void fb_term_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            if (cursor_y < rows && cursor_x < cols) {
                screen_buf[cursor_y][cursor_x] = ' ';
                draw_char_at(' ', cursor_x, cursor_y);
            }
        }
        return;
    } else if (c == '\t') {
        int n = 4 - (cursor_x % 4);
        for (int i = 0; i < n; i++) fb_term_putc(' ');
        return;
    } else {
        if (cursor_y < rows && cursor_x < cols) {
            screen_buf[cursor_y][cursor_x] = c;
            draw_char_at(c, cursor_x, cursor_y);
        }
        cursor_x++;
        if (cursor_x >= cols) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= rows) {
        for (int r = 0; r < rows - 1; r++)
            for (int c = 0; c < cols; c++)
                screen_buf[r][c] = screen_buf[r + 1][c];
        for (int c = 0; c < cols; c++)
            screen_buf[rows - 1][c] = ' ';
        fb_term_redraw();
        cursor_y = rows - 1;
    }
}

void fb_term_write(const char* s) {
    while (*s) fb_term_putc(*s++);
}

void fb_term_draw_text_at(int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    int cx = x;
    int cy = y;
    while (*s) {
        char c = *s++;
        const uint8_t* glyph = font8x16[(uint8_t)c];
        for (int row = 0; row < CHAR_H; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < CHAR_W; col++) {
                uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
                fb_putpixel(cx + col, cy + row, color);
            }
        }
        cx += CHAR_W;
    }
}