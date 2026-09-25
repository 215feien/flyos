#include "window.h"
#include "fb.h"
#include "fb_term.h"
#include <stdint.h>

void window_init(window_t* win, int x, int y, int w, int h, const char* title) {
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->title = title;

    win->bg_color     = fb_rgb(40, 40, 50);
    win->title_bg     = fb_rgb(70, 100, 200);
    win->title_fg     = fb_rgb(255, 255, 255);
    win->border_color = fb_rgb(20, 20, 30);

    win->visible = 1;
    win->focused = 0;
    win->on_draw  = 0;
    win->on_click = 0;
    win->on_close = 0;
    win->on_move  = 0;
    win->next = 0;
}

void window_set_colors(window_t* win, uint32_t bg, uint32_t title_bg, uint32_t title_fg) {
    win->bg_color = bg;
    win->title_bg = title_bg;
    win->title_fg = title_fg;
}

/* 关闭按钮的矩形位置 */
static void close_btn_rect(window_t* win, int* bx, int* by) {
    *bx = win->x + win->w - WIN_BORDER - WIN_CLOSE_SIZE - 4;
    *by = win->y + WIN_BORDER + (WIN_TITLE_H - WIN_CLOSE_SIZE) / 2;
}

int window_close_hit(window_t* win, int px, int py) {
    if (!win->on_close) return 0;
    int bx, by;
    close_btn_rect(win, &bx, &by);
    return px >= bx && px < bx + WIN_CLOSE_SIZE &&
           py >= by && py < by + WIN_CLOSE_SIZE;
}

static void draw_close_button(window_t* win) {
    if (!win->on_close) return;

    int bx, by;
    close_btn_rect(win, &bx, &by);

    /* 红色底 */
    fb_fill_rect(bx, by, WIN_CLOSE_SIZE, WIN_CLOSE_SIZE, fb_rgb(200, 60, 60));

    /* 白色 X */
    uint32_t white = fb_rgb(255, 255, 255);
    for (int i = 2; i < WIN_CLOSE_SIZE - 2; i++) {
        fb_putpixel(bx + i, by + i, white);
        fb_putpixel(bx + WIN_CLOSE_SIZE - 1 - i, by + i, white);
    }
}

void window_draw(window_t* win) {
    if (!win->visible) return;

    /* 外边框 */
    fb_fill_rect(win->x, win->y, win->w, win->h, win->border_color);

    /* 标题栏：焦点时原色，非焦点暗一半 */
    uint32_t title_bg = win->focused
                        ? win->title_bg
                        : ((win->title_bg >> 1) & 0x7F7F7F7F);

    fb_fill_rect(win->x + WIN_BORDER, win->y + WIN_BORDER,
                 win->w - 2 * WIN_BORDER, WIN_TITLE_H, title_bg);

    /* 标题文字 */
    fb_term_draw_text_at(win->x + 8, win->y + 4, win->title,
                         win->title_fg, title_bg);

    /* 关闭按钮 */
    draw_close_button(win);

    /* 内容区 */
    int cy = win->y + WIN_BORDER + WIN_TITLE_H;
    int ch = win->h - 2 * WIN_BORDER - WIN_TITLE_H;
    fb_fill_rect(win->x + WIN_BORDER, cy,
                 win->w - 2 * WIN_BORDER, ch, win->bg_color);

    /* 应用自己的绘制 */
    if (win->on_draw) win->on_draw(win);
}

int window_hit(window_t* win, int px, int py) {
    return px >= win->x && px < win->x + win->w &&
           py >= win->y && py < win->y + win->h;
}