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
#include "fat16.h"
#include "ata.h"
#include "fb.h"
#include "fb_term.h"
#include "fb_cursor.h"
#include "panic.h"
#include "gui.h"
#include "term_window.h"
#include "sem.h"
#include "pci.h"
#include "e1000.h"

extern void user_enter(void* entry, uint64_t user_stack_top);
extern uint8_t _binary_user_init_elf_start[];
extern uint8_t _binary_user_init_elf_end[];
extern int kernel_spawn_child(void);
extern uint8_t _binary_user_hello_elf_start[];
extern uint8_t _binary_user_hello_elf_end[];

static uint64_t user_entry_addr = 0;

/* ===== GUI demo ===== */
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

static void demo_close(window_t* w) {
    (void)w;
    serial_printf("GUI: counter demo closing\n");
    /* 实际移除由 gui_remove_window 处理 */
}

/* ===== 子进程 ===== */
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

/* ===== 网络测试 ===== */
static void net_test(void) {
    serial_printf("=== NET test ===\n");

    uint32_t dns_result = 0;   /* 跨段保存 DNS 解析结果 */

    /* ===== 1. ARP ===== */
    e1000_send_arp_request();
    for (volatile uint64_t i = 0; i < 100000000ULL; i++);

    int got_arp = 0;
    for (int tries = 0; tries < 30; tries++) {
        uint8_t pkt[2048];
        int n = e1000_recv(pkt, sizeof(pkt));
        if (n >= 42 && pkt[12] == 0x08 && pkt[13] == 0x06 &&
            pkt[20] == 0x00 && pkt[21] == 0x02) {
            for (int i = 0; i < 6; i++) gateway_mac[i] = pkt[6 + i];
            serial_printf("ARP: gateway MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
                          gateway_mac[0], gateway_mac[1], gateway_mac[2],
                          gateway_mac[3], gateway_mac[4], gateway_mac[5]);
            got_arp = 1;
            break;
        }
        for (volatile int i = 0; i < 20000000; i++);
    }
    if (!got_arp) { serial_printf("ARP: no reply\n"); return; }

    /* ===== 2. ICMP ===== */
    e1000_ping(0x0A000202);
    for (volatile uint64_t i = 0; i < 100000000ULL; i++);

    for (int tries = 0; tries < 30; tries++) {
        uint8_t pkt[2048];
        int n = e1000_recv(pkt, sizeof(pkt));
        if (n >= 42 && pkt[12] == 0x08 && pkt[13] == 0x00 &&
            pkt[23] == 0x01 && pkt[34] == 0x00) {
            serial_printf("ICMP: ECHO REPLY received! (%d bytes)\n", n);
            break;
        }
        for (volatile int i = 0; i < 20000000; i++);
    }

    /* ===== 3. DNS ===== */
    e1000_dns_query("example.com");
    for (volatile uint64_t i = 0; i < 100000000ULL; i++);

    for (int tries = 0; tries < 30; tries++) {
        uint8_t pkt[2048];
        int n = e1000_recv(pkt, sizeof(pkt));
        if (n < 42) continue;
        if (pkt[12] == 0x08 && pkt[13] == 0x00 && pkt[23] == 0x11 &&
            pkt[34] == 0x00 && pkt[35] == 0x35 &&
            pkt[36] == 0x30 && pkt[37] == 0x39) {
            serial_printf("DNS: got reply %d bytes\n", n);
            uint8_t ip[4];
            if (e1000_dns_parse_reply(pkt + 42, n - 42, ip) == 0) {
                dns_result = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16)
                           | ((uint32_t)ip[2] << 8)  |  (uint32_t)ip[3];
                serial_printf("DNS: example.com = %u.%u.%u.%u\n",
                              ip[0], ip[1], ip[2], ip[3]);
                break;
            } else {
                serial_printf("DNS: parse failed\n");
            }
            break;
        }
        for (volatile int i = 0; i < 20000000; i++);
    }

    /* ===== 4. TCP SYN ===== */
    uint32_t tcp_dst = dns_result ? dns_result : 0x0A000202;

    serial_printf("=== TCP test ===\n");
    serial_printf("TCP: target = %u.%u.%u.%u\n",
                  (tcp_dst >> 24) & 0xFF, (tcp_dst >> 16) & 0xFF,
                  (tcp_dst >> 8) & 0xFF, tcp_dst & 0xFF);

    e1000_tcp_syn(tcp_dst, 80);
    for (volatile uint64_t i = 0; i < 100000000ULL; i++);

    for (int tries = 0; tries < 30; tries++) {
        uint8_t pkt[2048];
        int n = e1000_recv(pkt, sizeof(pkt));
        if (n >= 54 && pkt[12] == 0x08 && pkt[13] == 0x00 && pkt[23] == 0x06) {
            serial_printf("TCP: got TCP packet %d bytes\n", n);

            tcp_reply_t r;
            if (e1000_tcp_parse_reply(pkt + 34, n - 34, 0, 0, &r) == 0) {
                serial_printf("TCP: src_port=%u seq=0x%08x ack=0x%08x flags=0x%04x\n",
                              r.src_port, r.seq, r.ack, r.flags);

                if (r.flags & 0x0012) {
                    serial_printf("TCP: SYN-ACK received!  <-- 握手第一步成功\n");
                } else if (r.flags & 0x0004) {
                    serial_printf("TCP: RST (端口关闭)\n");
                }
            }
            break;
        }
        for (volatile int i = 0; i < 20000000; i++);
    }
    serial_printf("TCP: done\n");
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

    /* ===== PCI + e1000 ===== */
    pci_init();

    pci_device_t* nic = pci_find(0x8086, 0x100E);
    if (nic) {
        serial_printf("=== e1000 found at %02x:%02x.%u, IRQ=%u ===\n",
                      nic->bus, nic->slot, nic->func, nic->irq_line);
        for (int i = 0; i < 6; i++) {
            serial_printf("  BAR%d = 0x%08x\n", i, nic->bar[i]);
        }

        uint64_t mmio_phys = nic->bar[0] & ~0xFULL;
        uint64_t mmio_size = 128 * 1024;
        uint64_t mmio_virt = 0xFFFFA00000000000ULL;

        for (uint64_t off = 0; off < mmio_size; off += 0x1000) {
            vmm_map(mmio_virt + off, mmio_phys + off, VMM_WRITABLE);
        }
        serial_printf("=== MMIO mapped: phys 0x%lx -> virt 0x%lx ===\n",
                      mmio_phys, mmio_virt);

        e1000_probe(mmio_virt);
        if (e1000_init() == 0) {
            net_test();
        }
    } else {
        serial_printf("=== e1000 NOT found ===\n");
    }

    fb_init(mb_info);
    fb_term_init();
    fb_term_write("Hello x86_64 OS!\n");
    fb_term_write("(graphics mode)\n\n");

    ata_init();

    ramfs_init();
    file_init();
    /* persist_load(); */

    fat16_init(0);
    serial_printf("=== FAT16 test ===\n");
    static char fatbuf[1024];
    int fatn = fat16_list_root(fatbuf, sizeof(fatbuf));
    if (fatn > 0) {
        serial_printf("FAT16 root:\n");
        for (int i = 0; i < fatn; i++) serial_putc(fatbuf[i]);
    } else {
        serial_printf("FAT16 root: (empty)\n");
    }

    mouse_init();
    fb_cursor_init();

    gui_init();

    static window_t term_win;
    term_window_create(&term_win, 50, 50, 700, 500, "Terminal");
    gui_add_window(&term_win);

    fb_term_write("Hello x86_64 OS!\n");
    fb_term_write("flyos terminal ready\n\n");

    window_init(&demo_win, 800, 100, 420, 300, "Counter Demo");
    window_set_colors(&demo_win, fb_rgb(40, 40, 55),
                                 fb_rgb(70, 100, 200),
                                 fb_rgb(255, 255, 255));
    demo_win.on_draw  = demo_draw;
    demo_win.on_click = demo_click;
    demo_win.on_close = demo_close;
    gui_add_window(&demo_win);

    term_win.focused = 1;
    gui_redraw();

    serial_printf("GUI: terminal + counter created\n");

    uint8_t* elf = _binary_user_init_elf_start;
    uint64_t elf_size = (uint64_t)(_binary_user_init_elf_end - _binary_user_init_elf_start);
    serial_printf("ELF size = %lu bytes\n", elf_size);

    user_entry_addr = user_load_elf(elf, elf_size);
    if (!user_entry_addr) {
        serial_printf("ELF load FAILED\n");
        for (;;) __asm__ volatile ("hlt");
    }

    user_setup_stack();

    task_init();
    sem_init_all();
    sem_init(0, 1);

    task_create("user", user_task_entry);

    serial_printf("=== starting scheduler ===\n");
    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}