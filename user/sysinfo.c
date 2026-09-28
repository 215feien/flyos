#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"

void _start(void) {
    puts("=== system info ===\n\n");

    uint64_t info[8];
    sys_sysinfo(info);

    uint64_t total = info[0];
    uint64_t used  = info[1];
    uint64_t free_ = total - used;

    printf("total memory: %lu pages (%lu MB)\n",
           total, total * 4 / 1024);
    printf("used memory:  %lu pages (%lu KB)\n",
           used, used * 4);
    printf("free memory:  %lu pages\n", free_);
    printf("children:     %lu\n", info[3]);
    printf("\n");

    puts("press enter to exit\n");
    char line[16];
    readline(line, sizeof(line));
    exit(0);
}
