#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"

static void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        puts(argv[i]);
    }
    putchar('\n');
}

static void cmd_help(void) {
    puts("built-in commands:\n");
    puts("  echo [args...]      print args\n");
    puts("  help                show this\n");
    puts("  printf              demo printf\n");
    puts("  ls [path]           list files\n");
    puts("  mkdir <dir>         create directory\n");
    puts("  cd <dir>            change directory\n");
    puts("  pwd                 print working directory\n");
    puts("  cat <file>          print file contents\n");
    puts("  write <file> <txt>  write text to file\n");
    puts("  rm <file>           remove file\n");
    puts("  sync                write fs to disk\n");
    puts("  sleep <ms>          sleep milliseconds\n");
    puts("  exit                quit shell\n");
}

static void cmd_printf_demo(void) {
    printf("int:  %d\n", -12345);
    printf("hex:  %x\n", 0xDEADBEEF);
    printf("long: %lu\n", 0x1234567890ABCDEFUL);
    printf("str:  %s\n", "hello world");
    printf("mix:  %s=%d (0x%x)\n", "answer", 42, 42);
}

static void cmd_ls(int argc, char* argv[]) {
    static char buf[2048];
    long n;
    if (argc >= 2) n = fs_ls_path(argv[1], buf, sizeof(buf));
    else           n = fs_ls(buf, sizeof(buf));
    if (n <= 0) { puts("(empty)\n"); return; }
    for (long i = 0; i < n; i++) putchar(buf[i]);
}

static void cmd_mkdir(const char* name) {
    if (fs_mkdir(name) == 0) printf("created %s\n", name);
    else printf("mkdir: %s failed\n", name);
}

static void cmd_cd(const char* name) {
    if (fs_chdir(name) == 0) return;
    printf("cd: %s: no such directory\n", name);
}

static void cmd_pwd(void) {
    char buf[256];
    fs_getcwd(buf, sizeof(buf));
    puts(buf);
    putchar('\n');
}

static void cmd_cat(const char* name) {
    int fd = fs_open(name);
    if (fd < 0) { printf("cat: cannot open %s\n", name); return; }
    char buf[256];
    long n;
    while ((n = fs_read(fd, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n; i++) putchar(buf[i]);
    }
    fs_close(fd);
}

static void cmd_write(int argc, char* argv[]) {
    if (argc < 3) { puts("usage: write <file> <text...>\n"); return; }
    int fd = fs_open(argv[1]);
    if (fd < 0) { printf("write: cannot open %s\n", argv[1]); return; }
    for (int i = 2; i < argc; i++) {
        if (i > 2) fs_write(fd, " ", 1);
        fs_write(fd, argv[i], (long)strlen(argv[i]));
    }
    fs_write(fd, "\n", 1);
    fs_close(fd);
    printf("written to %s\n", argv[1]);
}

static void cmd_rm(const char* name) {
    if (fs_unlink(name) == 0) printf("removed %s\n", name);
    else printf("rm: %s not found\n", name);
}

static void cmd_sync(void) {
    if (fs_sync() == 0) puts("filesystem synced to disk\n");
    else puts("sync failed\n");
}

static int atoi_(const char* s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static void cmd_sleep(int argc, char* argv[]) {
    if (argc < 2) { puts("usage: sleep <ms>\n"); return; }
    int ms = atoi_(argv[1]);
    printf("sleeping %d ms...\n", ms);
    sys_sleep(ms);
    printf("awake!\n");
}

static int tokenize(char* line, char* argv[], int max) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return argc;
}

void _start(void) {
    puts("flyos shell v0.6\n");
    puts("type 'help' for commands\n\n");

    char* line = (char*)malloc(128);
    char** argv = (char**)malloc(16 * sizeof(char*));

    for (;;) {
        puts("flyos> ");
        int len = readline(line, 128);
        if (len == 0) continue;

        int argc = tokenize(line, argv, 16);
        if (argc == 0) continue;

        if      (strcmp(argv[0], "echo")   == 0) cmd_echo(argc, argv);
        else if (strcmp(argv[0], "help")   == 0) cmd_help();
        else if (strcmp(argv[0], "printf") == 0) cmd_printf_demo();
        else if (strcmp(argv[0], "ls")     == 0) cmd_ls(argc, argv);
        else if (strcmp(argv[0], "mkdir")  == 0) {
            if (argc < 2) puts("usage: mkdir <dir>\n");
            else cmd_mkdir(argv[1]);
        }
        else if (strcmp(argv[0], "cd")     == 0) {
            if (argc < 2) puts("usage: cd <dir>\n");
            else cmd_cd(argv[1]);
        }
        else if (strcmp(argv[0], "pwd")    == 0) cmd_pwd();
        else if (strcmp(argv[0], "cat")    == 0) {
            if (argc < 2) puts("usage: cat <file>\n");
            else cmd_cat(argv[1]);
        }
        else if (strcmp(argv[0], "write")  == 0) cmd_write(argc, argv);
        else if (strcmp(argv[0], "rm")     == 0) {
            if (argc < 2) puts("usage: rm <file>\n");
            else cmd_rm(argv[1]);
        }
        else if (strcmp(argv[0], "sync")   == 0) cmd_sync();
        else if (strcmp(argv[0], "sleep")  == 0) cmd_sleep(argc, argv);
        else if (strcmp(argv[0], "exit")   == 0) exit(0);
        else printf("unknown command: %s\n", argv[0]);
    }
}