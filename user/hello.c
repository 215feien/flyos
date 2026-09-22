#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

void _start(void) {
    for (int i = 0; i < 10; i++) {
        printf("[child] hello, i=%d\n", i);
        sys_sleep(700);
    }
    puts("[child] done, exiting\n");
    exit(0);
}