#ifndef MYOS_FB_CURSOR_H
#define MYOS_FB_CURSOR_H

void fb_cursor_init(void);
void fb_cursor_move(int dx, int dy);
void fb_cursor_hide(void);
void fb_cursor_show(void);
void fb_cursor_get_pos(int* x, int* y);

#endif