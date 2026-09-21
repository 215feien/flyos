#include "gui.h"
#include "fb.h"
#include "fb_cursor.h"
#include "mouse.h"
#include "serial.h"
#include <stdint.h>

#define MAX_WINDOWS 8

static window_t* windows[MAX_WINDOWS];
static int window_count = 0;

static int last_buttons = 0;

void gui_init(void) {
    window_count = 0;
    last_buttons = 0;
}

void gui_add_window(window_t* win) {
    if (window_count >= MAX_WINDOWS) return;
    windows[window_count++] = win;
}

void gui_redraw(void) {
    fb_cursor_hide();

    /* 清屏 */
    fb_clear(fb_rgb(20, 20, 40));

    /* 依次绘制窗口（后加的在上层，简单起见） */
    for (int i = 0; i < window_count; i++) {
        window_draw(windows[i]);
    }

    fb_cursor_show();
}

window_t* gui_top_window_at(int x, int y) {
    for (int i = window_count - 1; i >= 0; i--) {
        if (window_hit(windows[i], x, y)) return windows[i];
    }
    return 0;
}

void gui_on_mouse(void) {
    int mx, my;
    fb_cursor_get_pos(&mx, &my);

    uint8_t b = mouse_buttons();
    int left_now  = (b & 0x01) ? 1 : 0;
    int left_prev = (last_buttons & 0x01) ? 1 : 0;

    if (left_now && !left_prev) {
        window_t* w = gui_top_window_at(mx, my);
        if (w) {
            /* 点击窗口内部：调用窗口的回调 */
            if (w->on_click) {
                w->on_click(w, mx, my);
            }
            /* 立刻重绘 */
            gui_redraw();
        }
    }

    last_buttons = b;
}