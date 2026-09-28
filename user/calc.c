#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"

static int atoi_(const char* s, int* ok) {
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    if (*s < '0' || *s > '9') { *ok = 0; return 0; }
    int n = 0;
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    *ok = 1;
    return neg ? -n : n;
}

void _start(void) {
    puts("=== flyos calc ===\n");
    puts("enter: <num> <op> <num>\n");
    puts("ops: + - * /      exit: q\n\n");

    char line[64];
    for (;;) {
        puts("> ");
        int len = readline(line, sizeof(line));
        if (len == 0) continue;
        if (line[0] == 'q' && line[1] == 0) { sys_exec("shell"); break; }

        char* p = line;
        while (*p == ' ') p++;
        char* a_str = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
        while (*p == ' ') p++;
        char op = *p;
        if (!op) { puts("syntax error\n"); continue; }
        p++;
        while (*p == ' ') p++;
        char* b_str = p;

        int ok1 = 0, ok2 = 0;
        int a = atoi_(a_str, &ok1);
        int b = atoi_(b_str, &ok2);
        if (!ok1 || !ok2) { puts("invalid number\n"); continue; }

        int r = 0;
        switch (op) {
            case '+': r = a + b; break;
            case '-': r = a - b; break;
            case '*': r = a * b; break;
            case '/':
                if (b == 0) { puts("divide by zero\n"); continue; }
                r = a / b;
                break;
            default: puts("unknown op\n"); continue;
        }
        printf("%d %c %d = %d\n", a, op, b, r);
    }
    puts("bye\n");
    exit(0);
}
