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
    win->next = 0;
    win->on_move  = 0;
}

void window_set_colors(window_t* win, uint32_t bg, uint32_t title_bg, uint32_t title_fg) {
    win->bg_color = bg;
    win->title_bg = title_bg;
    win->title_fg = title_fg;
}

void window_draw(window_t* win) {
    if (!win->visible) return;

    fb_fill_rect(win->x, win->y, win->w, win->h, win->border_color);

    /* 焦点窗口用原色，非焦点变暗 */
    uint32_t title_bg = win->focused
                        ? win->title_bg
                        : ((win->title_bg >> 1) & 0x7F7F7F7F);

    fb_fill_rect(win->x + WIN_BORDER, win->y + WIN_BORDER,
                 win->w - 2 * WIN_BORDER, WIN_TITLE_H, title_bg);

    fb_term_draw_text_at(win->x + 8, win->y + 4, win->title, win->title_fg, title_bg);

    int cy = win->y + WIN_BORDER + WIN_TITLE_H;
    int ch = win->h - 2 * WIN_BORDER - WIN_TITLE_H;
    fb_fill_rect(win->x + WIN_BORDER, cy,
                 win->w - 2 * WIN_BORDER, ch, win->bg_color);

    if (win->on_draw) win->on_draw(win);
}

int window_hit(window_t* win, int px, int py) {
    return px >= win->x && px < win->x + win->w &&
           py >= win->y && py < win->y + win->h;
}