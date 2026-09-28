#include "apps.h"
#include "serial.h"
#include <stdint.h>

extern uint8_t _binary_user_init_elf_start[];
extern uint8_t _binary_user_init_elf_end[];
extern uint8_t _binary_user_hello_elf_start[];
extern uint8_t _binary_user_hello_elf_end[];
extern uint8_t _binary_user_calc_elf_start[];
extern uint8_t _binary_user_calc_elf_end[];

static app_entry_t app_table[] = {
    { "shell", _binary_user_init_elf_start,  _binary_user_init_elf_end  },
    { "hello", _binary_user_hello_elf_start, _binary_user_hello_elf_end },
    { "calc",  _binary_user_calc_elf_start,  _binary_user_calc_elf_end  },
};
static const int app_count = sizeof(app_table) / sizeof(app_table[0]);

static int str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void apps_init(void) {
    serial_printf("APPS: %d applications registered\n", app_count);
    for (int i = 0; i < app_count; i++) {
        uint64_t sz = (uint64_t)(app_table[i].elf_end - app_table[i].elf_start);
        serial_printf("APPS:   %s (%lu bytes)\n", app_table[i].name, sz);
    }
}

const app_entry_t* apps_find(const char* name) {
    for (int i = 0; i < app_count; i++) {
        if (str_eq(app_table[i].name, name)) return &app_table[i];
    }
    return 0;
}
