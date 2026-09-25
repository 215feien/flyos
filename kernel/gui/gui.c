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

static window_t* focus = 0;
static window_t* drag_win = 0;
static int drag_off_x = 0;
static int drag_off_y = 0;

void gui_init(void) {
    window_count = 0;
    last_buttons = 0;
    focus = 0;
    drag_win = 0;
}

void gui_add_window(window_t* win) {
    if (window_count >= MAX_WINDOWS) return;
    windows[window_count++] = win;
}

void gui_remove_window(window_t* win) {
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return;
    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    window_count--;
    if (focus == win) {
        focus = (window_count > 0) ? windows[window_count - 1] : 0;
        if (focus) focus->focused = 1;
    }
    if (drag_win == win) drag_win = 0;
}

static void bring_to_front(window_t* win) {
    if (window_count == 0) return;
    if (win == windows[window_count - 1]) return;

    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return;
    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    windows[window_count - 1] = win;
}

void gui_redraw(void) {
    fb_cursor_hide();
    fb_clear(fb_rgb(20, 20, 40));

    for (int i = 0; i < window_count; i++) {
        if (windows[i]->visible) {
            windows[i]->focused = (windows[i] == focus);
            window_draw(windows[i]);
        }
    }
    fb_cursor_show();
}

window_t* gui_top_window_at(int x, int y) {
    for (int i = window_count - 1; i >= 0; i--) {
        if (windows[i]->visible && window_hit(windows[i], x, y))
            return windows[i];
    }
    return 0;
}

static int in_title_bar(window_t* w, int x, int y) {
    return x >= w->x && x < w->x + w->w &&
           y >= w->y && y < w->y + WIN_BORDER + WIN_TITLE_H;
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
            /* 关闭按钮？ */
            if (window_close_hit(w, mx, my)) {
                serial_printf("GUI: closing window '%s'\n", w->title);
                if (w->on_close) w->on_close(w);
                gui_remove_window(w);
                gui_redraw();
                last_buttons = b;
                return;
            }

            bring_to_front(w);
            if (focus) focus->focused = 0;
            focus = w;
            focus->focused = 1;

            if (in_title_bar(w, mx, my)) {
                drag_win = w;
                drag_off_x = mx - w->x;
                drag_off_y = my - w->y;
            }
            if (w->on_click) w->on_click(w, mx, my);
            gui_redraw();
        }
    }
    else if (!left_now && left_prev) {
        drag_win = 0;
    }
    else if (left_now && drag_win) {
        int nx = mx - drag_off_x;
        int ny = my - drag_off_y;
        if (nx != drag_win->x || ny != drag_win->y) {
            drag_win->x = nx;
            drag_win->y = ny;
            if (drag_win->on_move) drag_win->on_move(drag_win);
            gui_redraw();
        }
    }

    last_buttons = b;
}