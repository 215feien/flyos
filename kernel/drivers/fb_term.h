#ifndef MYOS_FB_TERM_H
#define MYOS_FB_TERM_H

#include <stdint.h>

void fb_term_init(void);
void fb_term_putc(char c);
void fb_term_write(const char* s);
void fb_term_clear(void);
void fb_term_set_color(uint32_t fg, uint32_t bg);
void fb_term_draw_text_at(int x, int y, const char* s, uint32_t fg, uint32_t bg);

/* 新增：输出区域控制 */
void fb_term_set_region(int x, int y, int w, int h);
void fb_term_redraw(void);

#endif