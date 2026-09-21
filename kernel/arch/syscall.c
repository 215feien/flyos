#include "syscall.h"
#include "serial.h"
#include "vga.h"
#include "keyboard.h"
#include "file.h"
#include "ramfs.h"
#include "persist.h"
#include "fb_term.h"
#include "task.h"
#include <stdint.h>

#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_FMASK  0xC0000084

extern void syscall_entry(void);

static inline void wrmsr(uint32_t msr, uint64_t v) {
    uint32_t lo = (uint32_t)v;
    uint32_t hi = (uint32_t)(v >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init(void) {
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x10 << 48);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_FMASK, 0x200 | 0x400 | 0x800 | 0x40000);

    serial_printf("SYSCALL: enabled, LSTAR=0x%lx\n", (uint64_t)syscall_entry);
}

uint64_t syscall_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3) {
    switch (nr) {
        case SYS_WRITE: {
            const char* buf = (const char*)a2;
            for (uint64_t i = 0; i < a3; i++) {
                serial_putc(buf[i]);
                fb_term_putc(buf[i]);
            }
            return a3;
        }

        case SYS_READ: {
            char* buf = (char*)a2;
            uint64_t want = a3, got = 0;
            while (got < want) {
                int c;
                while ((c = keyboard_getchar_nonblock()) < 0) {
                    __asm__ volatile ("sti; hlt");
                }
                buf[got++] = (char)c;
            }
            return got;
        }

        case SYS_EXIT:
            serial_printf("\nSYSCALL: exit(%lu)\n", a1);
            persist_save();
            for (;;) __asm__ volatile ("cli; hlt");

        case SYS_OPEN: {
            const char* name = (const char*)a1;
            return (uint64_t)(int64_t)file_open(name);
        }

        case SYS_CLOSE:
            return (uint64_t)(int64_t)file_close((int)a1);

        case SYS_FREAD:
            return (uint64_t)file_read((int)a1, (void*)a2, a3);

        case SYS_FWRITE:
            return (uint64_t)file_write((int)a1, (const void*)a2, a3);

        case SYS_UNLINK: {
            const char* name = (const char*)a1;
            return (uint64_t)(int64_t)ramfs_unlink(name);
        }

        case SYS_LS: {
            char* buf = (char*)a1;
            uint64_t max = a2;
            uint64_t pos = 0;
            for (ramfs_file_t* f = ramfs_first(); f; f = f->next) {
                const char* n = f->name;
                uint64_t l = 0;
                while (n[l]) l++;
                if (pos + l + 1 > max) break;
                for (uint64_t i = 0; i < l; i++) buf[pos++] = n[i];
                buf[pos++] = '\n';
            }
            return pos;
        }

        case SYS_SYNC:
            return (uint64_t)persist_save();

        case SYS_SLEEP:
            task_sleep(a1);
            return 0;

        default:
            serial_printf("SYSCALL: unknown %lu\n", nr);
            return (uint64_t)-1;
    }
}
