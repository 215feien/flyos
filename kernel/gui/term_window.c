#include "term_window.h"
#include "fb_term.h"
#include "fb.h"

static void term_on_draw(window_t* win) {
    (void)win;
    fb_term_redraw();
}

void term_window_create(window_t* win, int x, int y, int w, int h, const char* title) {
    window_init(win, x, y, w, h, title);
    window_set_colors(win, fb_rgb(10, 10, 40),
                             fb_rgb(70, 100, 200),
                             fb_rgb(255, 255, 255));
    win->on_draw = term_on_draw;

    int rx = win->x + WIN_BORDER;
    int ry = win->y + WIN_BORDER + WIN_TITLE_H;
    int rw = win->w - 2 * WIN_BORDER;
    int rh = win->h - 2 * WIN_BORDER - WIN_TITLE_H;
    fb_term_set_region(rx, ry, rw, rh);
}

void term_window_focus(window_t* win) {
    int rx = win->x + WIN_BORDER;
    int ry = win->y + WIN_BORDER + WIN_TITLE_H;
    int rw = win->w - 2 * WIN_BORDER;
    int rh = win->h - 2 * WIN_BORDER - WIN_TITLE_H;
    fb_term_set_region(rx, ry, rw, rh);
}
