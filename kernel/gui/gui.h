#ifndef MYOS_GUI_H
#define MYOS_GUI_H

#include "window.h"

void gui_init(void);
void gui_add_window(window_t* win);
void gui_redraw(void);
void gui_on_mouse(void);       /* 鼠标状态变化时调用 */
window_t* gui_top_window_at(int x, int y);

#endif