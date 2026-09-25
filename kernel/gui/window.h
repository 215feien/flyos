#ifndef MYOS_WINDOW_H
#define MYOS_WINDOW_H

#include <stdint.h>

#define WIN_TITLE_H 24
#define WIN_BORDER  2
#define WIN_CLOSE_SIZE 14

struct window;

typedef struct window {
    int          x, y, w, h;
    const char*  title;
    uint32_t     bg_color;
    uint32_t     title_bg;
    uint32_t     title_fg;
    uint32_t     border_color;
    int          visible;
    int          focused;

    void (*on_draw)(struct window*);
    void (*on_click)(struct window*, int mx, int my);
    void (*on_close)(struct window*);
    void (*on_move)(struct window*);

    struct window* next;
} window_t;

void window_init(window_t* win, int x, int y, int w, int h, const char* title);
void window_set_colors(window_t* win, uint32_t bg, uint32_t title_bg, uint32_t title_fg);
void window_draw(window_t* win);
int  window_hit(window_t* win, int px, int py);
int  window_close_hit(window_t* win, int px, int py);

#endif