#include "panic.h"
#include "serial.h"
#include <stdarg.h>
#include <stdint.h>

static void dump_stack(uint64_t rbp, uint64_t rip) {
    serial_printf("Stack trace:\n");
    serial_printf("  #00  rip=0x%lx\n", rip);

    int depth = 0;
    while (rbp && depth < 24) {
        /* 合理的栈范围检查：内核在 1MB 以上 */
        if (rbp < 0x100000 || rbp > 0x1000000000ULL) break;

        uint64_t* frame = (uint64_t*)rbp;
        uint64_t next_rbp = frame[0];
        uint64_t ret_addr = frame[1];

        if (ret_addr == 0) break;

        serial_printf("  #%02d  rip=0x%lx  rbp=0x%lx\n",
                      depth + 1, ret_addr, rbp);

        if (next_rbp <= rbp) break;   /* 栈必须向上增长（rbp 变大） */
        rbp = next_rbp;
        depth++;
    }
}

void panic(const char* fmt, ...) {
    __asm__ volatile ("cli");

    serial_write("\n");
    serial_write("================== KERNEL PANIC ==================\n");

    va_list ap;
    va_start(ap, fmt);
    serial_vprintf(fmt, ap);
    va_end(ap);
    serial_write("\n");

    uint64_t rbp, rip;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    __asm__ volatile ("lea (%%rip), %0" : "=r"(rip));

    serial_printf("RBP = 0x%lx   RIP = 0x%lx\n", rbp, rip);

    dump_stack(rbp, rip);

    serial_write("==================================================\n");
    serial_write("System halted.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void panic_assert_fail(const char* expr, const char* file, int line) {
    panic("ASSERT FAILED: %s\n  at %s:%d", expr, file, line);
}