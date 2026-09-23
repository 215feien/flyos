#include <stdint.h>
#include "serial.h"
#include "vga.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "irq.h"
#include "keyboard.h"
#include "mouse.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "task.h"
#include "gdt.h"
#include "syscall.h"
#include "user.h"
#include "ramfs.h"
#include "file.h"
#include "persist.h"
#include "ata.h"
#include "fb.h"
#include "fb_term.h"
#include "fb_cursor.h"
#include "panic.h"
#include "gui.h"
#include "term_window.h"
#include "sem.h"
#include "vmm.h"

extern void user_enter(void* entry, uint64_t user_stack_top);
extern uint8_t _binary_user_init_elf_start[];
extern uint8_t _binary_user_init_elf_end[];
extern uint8_t _binary_user_hello_elf_start[];
extern uint8_t _binary_user_hello_elf_end[];

static uint64_t user_entry_addr = 0;

/* ===== GUI demo：计数器 ===== */
static window_t demo_win;
static int      counter_value = 0;

#define BTN_X_OFF 30
#define BTN_Y_OFF 40
#define BTN_W     180
#define BTN_H     48

static int button_hit(window_t* w, int mx, int my) {
    int bx = w->x + BTN_X_OFF;
    int by = w->y + WIN_TITLE_H + WIN_BORDER + BTN_Y_OFF;
    return mx >= bx && mx < bx + BTN_W &&
           my >= by && my < by + BTN_H;
}

static void demo_draw(window_t* w) {
    int bx = w->x + BTN_X_OFF;
    int by = w->y + WIN_TITLE_H + WIN_BORDER + BTN_Y_OFF;

    fb_fill_rect(bx, by, BTN_W, BTN_H, fb_rgb(50, 160, 80));
    fb_term_draw_text_at(bx + 20, by + 16, "Click me!",
                         fb_rgb(255, 255, 255), fb_rgb(50, 160, 80));

    char buf[16];
    int n = counter_value;
    int i = 0;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = (char)('0' + (n % 10)); n /= 10; }
    for (int a = 0, b = i - 1; a < b; a++, b--) {
        char t = buf[a]; buf[a] = buf[b]; buf[b] = t;
    }
    buf[i] = 0;

    int tx = w->x + BTN_X_OFF;
    int ty = by + BTN_H + 40;
    fb_term_draw_text_at(tx, ty, "Count:", fb_rgb(200, 200, 200), w->bg_color);
    fb_term_draw_text_at(tx + 8 * 6, ty, buf, fb_rgb(255, 220, 60), w->bg_color);
}

static void demo_click(window_t* w, int mx, int my) {
    if (button_hit(w, mx, my)) {
        counter_value++;
        serial_printf("GUI: button clicked, counter = %d\n", counter_value);
    } else {
        serial_printf("GUI: clicked demo window (not button) at (%d,%d)\n", mx, my);
    }
}

static void timer_handler(struct regs* r) {
    (void)r;
    gui_on_mouse();
    scheduler_tick();
}

static void user_task_entry(void) {
    serial_printf("[kernel] entering ring3 at 0x%lx\n", user_entry_addr);
    user_enter((void*)user_entry_addr, USER_STACK_BASE + USER_STACK_SIZE);
    for (;;) { __asm__ volatile ("hlt"); }
}

static int spawn_used = 0;

static void child_task_entry(void) {
    serial_printf("[kernel] child entering ring3 at 0x%lx\n", CHILD_CODE_BASE);
    user_enter((void*)CHILD_CODE_BASE, CHILD_STACK_BASE + CHILD_STACK_SIZE);
    for (;;) { __asm__ volatile ("hlt"); }
}

int kernel_spawn_child(void) {
    if (spawn_used) return -1;
    spawn_used = 1;

    uint64_t sz = (uint64_t)(_binary_user_hello_elf_end - _binary_user_hello_elf_start);
    uint64_t entry = user_load_elf(_binary_user_hello_elf_start, sz);
    if (!entry) {
        serial_printf("SPAWN: load failed\n");
        return -1;
    }
    user_setup_stack_at(CHILD_STACK_BASE, CHILD_STACK_SIZE);
    task_t* t = task_create("child", child_task_entry);
    serial_printf("SPAWN: child task created (entry=0x%lx) id=%u\n", entry, t->id);
    return (int)t->id;
}

void kmain(uint32_t mb_info, uint32_t magic) {
    (void)magic;

    serial_init();
    serial_printf("Hello serial\n");

    gdt_init();
    syscall_init();

    idt_init();
    pic_init();
    irq_init();
    irq_register(0, timer_handler);
    pit_init(100);
    pic_clear_mask(0);
    keyboard_init();
    pic_clear_mask(1);
    serial_printf("init done\n");

    pmm_init(mb_info);
    vmm_init();
    heap_init();

    fb_init(mb_info);
    fb_term_init();

    ata_init();

    ramfs_init();
    file_init();
    persist_load();

    mouse_init();
    fb_cursor_init();

    /* ===== GUI ===== */
    gui_init();

    /* 终端窗口：shell 输出进这里 */
    gui_init();

    static window_t term_win;
    term_window_create(&term_win, 50, 50, 700, 500, "Terminal");
    gui_add_window(&term_win);

    window_init(&demo_win, 800, 100, 420, 300, "Counter Demo");
    window_set_colors(&demo_win, fb_rgb(40, 40, 55),
                                 fb_rgb(70, 100, 200),
                                 fb_rgb(255, 255, 255));
    demo_win.on_draw  = demo_draw;
    demo_win.on_click = demo_click;
    gui_add_window(&demo_win);

    term_win.focused = 1;   /* 终端默认聚焦 */
    gui_redraw();

    /* 欢迎语写在终端窗口里 */
    fb_term_write("Hello x86_64 OS!\n");
    fb_term_write("flyos terminal ready\n\n");

    /* 计数器窗口 */
    window_init(&demo_win, 800, 100, 420, 300, "Counter Demo");
    window_set_colors(&demo_win, fb_rgb(40, 40, 55),
                                 fb_rgb(70, 100, 200),
                                 fb_rgb(255, 255, 255));
    demo_win.on_draw  = demo_draw;
    demo_win.on_click = demo_click;
    gui_add_window(&demo_win);

    gui_redraw();

    serial_printf("GUI: terminal + counter created\n");

    /* 加载用户 ELF */
    uint8_t* elf = _binary_user_init_elf_start;
    uint64_t elf_size = (uint64_t)(_binary_user_init_elf_end - _binary_user_init_elf_start);
    serial_printf("ELF size = %lu bytes\n", elf_size);

    user_entry_addr = user_load_elf(elf, elf_size);
    if (!user_entry_addr) {
        serial_printf("ELF load FAILED\n");
        for (;;) __asm__ volatile ("hlt");
    }

    user_setup_stack();

    /* 给 user 任务一份独立的页表 */
    uint64_t user_pml4 = vmm_clone_pml4();

    task_init();
    sem_init_all();
    sem_init(0, 1);      /* id 0 = 终端输出的互斥锁，初值 1 */

    task_t* user_task = task_create("user", user_task_entry);
    user_task->pml4_phys = user_pml4;

    serial_printf("=== starting scheduler ===\n");
    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}