#ifndef MYOS_TERM_WINDOW_H
#define MYOS_TERM_WINDOW_H

#include "window.h"

void term_window_create(window_t* win, int x, int y, int w, int h, const char* title);
void term_window_focus(window_t* win);

#endif