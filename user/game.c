#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"

static uint32_t seed = 12345;

static uint32_t rng(void) {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

static int atoi_(const char* s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    return n;
}

void _start(void) {
    puts("=== guess the number ===\n");
    puts("I'm thinking of a number 1-100\n\n");

    int secret = (int)(rng() % 100) + 1;
    int tries = 0;
    char line[16];

    for (;;) {
        puts("your guess: ");
        int len = readline(line, sizeof(line));
        if (len == 0) continue;
        if (line[0] == 'q') break;

        int g = atoi_(line);
        if (g < 1 || g > 100) { puts("1-100 please\n"); continue; }
        tries++;

        if (g < secret) puts("too low, go higher!\n");
        else if (g > secret) puts("too high, go lower!\n");
        else {
            printf("correct! %d tries\n", tries);
            break;
        }
    }
    puts("bye\n");
    exit(0);
}
