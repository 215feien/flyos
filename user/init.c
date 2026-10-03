#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"

/* 前向声明 */
static void dispatch(int argc, char* argv[]);

/* ================= 命令实现 ================= */

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
    puts("  fork                fork this process\n");
    puts("  fatls               list FAT16 root\n");
    puts("  fatcat <file>       print FAT16 file\n");
    puts("  fatwrite <f> <txt>  write FAT16 file\n");
    puts("  fatrm <file>        delete FAT16 file\n");
    puts("  edit <file>         text editor\n");
    puts("  ps                  list tasks\n");
    puts("  kill <pid>          kill a task\n");
    puts("  run <app>           run app (shell, hello, calc, game, sysinfo)\n");
    puts("  spawn               spawn a child process\n");
    puts("  <cmd> > <file>      redirect stdout\n");
    puts("  <cmd> < <file>      redirect stdin\n");
    puts("  <a> | <b>           pipe\n");
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

static void cmd_cat_stdin(void) {
    char buf[256];
    long n;
    while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n; i++) putchar(buf[i]);
    }
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

static void cmd_fatls(void) {
    static char buf[2048];
    long n = fs_fat_ls(buf, sizeof(buf));
    if (n <= 0) { puts("(empty)\n"); return; }
    for (long i = 0; i < n; i++) putchar(buf[i]);
}

static void cmd_fatcat(const char* name) {
    char buf[512];
    long n = fs_fat_read(name, buf, sizeof(buf));
    if (n < 0) { printf("fatcat: %s not found\n", name); return; }
    for (long i = 0; i < n; i++) putchar(buf[i]);
    putchar('\n');
}

static void cmd_fatwrite(int argc, char* argv[]) {
    if (argc < 3) { puts("usage: fatwrite <file> <text...>\n"); return; }
    static char buf[512];
    int pos = 0;
    for (int i = 2; i < argc; i++) {
        if (i > 2 && pos < 511) buf[pos++] = ' ';
        const char* ss = argv[i];
        while (*ss && pos < 510) buf[pos++] = *ss++;
    }
    buf[pos++] = '\n';
    buf[pos] = 0;
    if (fs_fat_write(argv[1], buf, pos) == 0)
        printf("written to %s\n", argv[1]);
    else
        printf("fatwrite: %s failed\n", argv[1]);
}

static void cmd_fatrm(const char* name) {
    if (fs_fat_delete(name) == 0) printf("removed %s\n", name);
    else printf("fatrm: %s not found\n", name);
}

static void cmd_edit(int argc, char* argv[]) {
    if (argc < 2) { puts("usage: edit <file>\n"); return; }

    /* 自动创建父目录 */
    const char* name = argv[1];
    int last_slash = -1;
    for (int i = 0; name[i]; i++) if (name[i] == '/') last_slash = i;
    if (last_slash > 0) {
        char parent[128];
        int plen = last_slash;
        if (plen > 127) plen = 127;
        for (int i = 0; i < plen; i++) parent[i] = name[i];
        parent[plen] = 0;
        fs_mkdir(parent);
    }

    /* 读已有内容 */
    char oldbuf[1024];
    long oldn = 0;
    int fd = fs_open(name);
    if (fd >= 0) {
        oldn = fs_read(fd, oldbuf, sizeof(oldbuf) - 1);
        fs_close(fd);
        if (oldn < 0) oldn = 0;
    }
    oldbuf[oldn] = 0;

    puts("\n=== flyos editor ===\n");
    if (oldn > 0) {
        puts("current content:\n");
        puts(oldbuf);
        if (oldbuf[oldn - 1] != '\n') putchar('\n');
    } else {
        puts("(new file)\n");
    }
    puts("\nenter new content, blank line to finish:\n\n");

    static char newbuf[2048];
    int pos = 0;
    for (;;) {
        puts("> ");
        char line[128];
        int len = readline(line, sizeof(line));
        if (len == 0) break;
        for (int i = 0; i < len && pos < 2047; i++) {
            newbuf[pos++] = line[i];
        }
        if (pos < 2047) newbuf[pos++] = '\n';
    }
    newbuf[pos] = 0;

    if (pos == 0) {
        puts("(no changes, keeping original)\n");
        return;
    }

    fs_unlink(name);
    int fd2 = fs_open(name);
    if (fd2 < 0) { puts("edit: write failed\n"); return; }
    fs_write(fd2, newbuf, pos);
    fs_close(fd2);
    fs_sync();

    printf("\nsaved %d bytes to %s\n", pos, name);
}

static void cmd_ps(void) {
    static char buf[1024];
    long n = sys_ps(buf, sizeof(buf));
    if (n <= 0) { puts("(no tasks)\n"); return; }
    for (long i = 0; i < n; i++) putchar(buf[i]);
}

static void cmd_kill(int argc, char* argv[]) {
    if (argc < 2) { puts("usage: kill <pid>\n"); return; }
    int pid = 0;
    for (const char* s = argv[1]; *s >= '0' && *s <= '9'; s++) pid = pid * 10 + (*s - '0');
    if (sys_kill(pid) == 0) printf("killed pid %d\n", pid);
    else printf("kill: pid %d not found\n", pid);
}

static void cmd_run(int argc, char* argv[]) {
    if (argc < 2) { puts("usage: run <app>\n"); return; }

    i64 pid = sys_fork();
    if (pid == 0) {
        sys_set_fg();
        sys_exec(argv[1]);
        printf("run: %s failed\n", argv[1]);
        exit(1);
    } else if (pid > 0) {
        printf("started %s as pid %ld (shell suspended)\n", argv[1], (long)pid);
        sys_wait();
        printf("\n[child exited, back to shell]\n\n");
    } else {
        puts("fork failed\n");
    }
}

/* ================= 管道和重定向 ================= */

static void cmd_redirect(char** argv, int argc, const char* file, int which) {
    i64 pid = sys_fork();
    if (pid == 0) {
        sys_set_fg();
        sys_redir(which, file);
        dispatch(argc, argv);
        exit(0);
    } else if (pid > 0) {
        sys_wait();
    }
}

static void cmd_pipe(char** argv, int argc, int pipe_idx) {
    char* left[16];
    char* right[16];
    int li = 0, ri = 0;

    for (int i = 0; i < pipe_idx && li < 15; i++) left[li++] = argv[i];
    left[li] = 0;
    for (int i = pipe_idx + 1; i < argc && ri < 15; i++) right[ri++] = argv[i];
    right[ri] = 0;

    const char* tmp = "/tmp/_pipe";

    /* 左：stdout 重定向到 tmp */
    cmd_redirect(left, li, tmp, 1);
    /* 右：stdin 重定向从 tmp */
    cmd_redirect(right, ri, tmp, 0);

    fs_unlink(tmp);
}

/* ================= 词法分析 ================= */

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

/* ================= 命令分派 ================= */

static void dispatch(int argc, char* argv[]) {
    if (argc == 0) return;
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
        if (argc >= 2) cmd_cat(argv[1]);
        else cmd_cat_stdin();
    }
    else if (strcmp(argv[0], "write")  == 0) cmd_write(argc, argv);
    else if (strcmp(argv[0], "rm")     == 0) {
        if (argc < 2) puts("usage: rm <file>\n");
        else cmd_rm(argv[1]);
    }
    else if (strcmp(argv[0], "sync")   == 0) cmd_sync();
    else if (strcmp(argv[0], "sleep")  == 0) cmd_sleep(argc, argv);
    else if (strcmp(argv[0], "fatls")  == 0) cmd_fatls();
    else if (strcmp(argv[0], "fatcat") == 0) {
        if (argc < 2) puts("usage: fatcat <file>\n");
        else cmd_fatcat(argv[1]);
    }
    else if (strcmp(argv[0], "fatwrite") == 0) cmd_fatwrite(argc, argv);
    else if (strcmp(argv[0], "fatrm")    == 0) {
        if (argc < 2) puts("usage: fatrm <file>\n");
        else cmd_fatrm(argv[1]);
    }
    else if (strcmp(argv[0], "edit")   == 0) cmd_edit(argc, argv);
    else if (strcmp(argv[0], "ps")     == 0) cmd_ps();
    else if (strcmp(argv[0], "kill")   == 0) cmd_kill(argc, argv);
    else if (strcmp(argv[0], "run")    == 0) cmd_run(argc, argv);
    else if (strcmp(argv[0], "spawn")  == 0) {
        if (sys_spawn() == 0) puts("spawned child\n");
        else puts("spawn failed\n");
    }
    else if (strcmp(argv[0], "exit")   == 0) exit(0);
    else printf("unknown command: %s\n", argv[0]);
}

/* ================= 主循环 ================= */

void _start(void) {
    puts("flyos shell v0.8\n");
    puts("type 'help' for commands\n\n");

    fs_mkdir("/tmp");

    char* line = (char*)malloc(128);
    char** argv = (char**)malloc(16 * sizeof(char*));

    for (;;) {
        puts("flyos> ");
        int len = readline(line, 128);
        if (len == 0) continue;

        int argc = tokenize(line, argv, 16);
        if (argc == 0) continue;

        /* 找第一个重定向 > 或 < */
        int redir = -1;
        int redir_kind = 0;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
                redir = i; redir_kind = 1; break;
            }
            if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
                redir = i; redir_kind = 0; break;
            }
        }
        if (redir >= 0) {
            const char* file = argv[redir + 1];
            argv[redir] = 0;
            cmd_redirect(argv, redir, file, redir_kind);
            continue;
        }

        /* 找管道 | */
        int pipe_idx = -1;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "|") == 0) { pipe_idx = i; break; }
        }
        if (pipe_idx > 0) {
            cmd_pipe(argv, argc, pipe_idx);
            continue;
        }

        dispatch(argc, argv);
    }
}
