#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

void _start(void) {
    for (int i = 0; i < 20; i++) {
        lock_stdout();
        printf("[child] hello, i=%d\n", i);
        unlock_stdout();
        sys_sleep(300);
    }
    puts("[child] done\n");
    exit(0);
}