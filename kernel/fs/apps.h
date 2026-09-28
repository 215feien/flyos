#ifndef MYOS_APPS_H
#define MYOS_APPS_H

#include <stdint.h>

typedef struct {
    const char*     name;
    const uint8_t*  elf_start;
    const uint8_t*  elf_end;
} app_entry_t;

void apps_init(void);
const app_entry_t* apps_find(const char* name);

#endif
