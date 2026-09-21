#ifndef USER_STDIO_H
#define USER_STDIO_H

void putchar(char c);
void puts(const char* s);
int  printf(const char* fmt, ...);
char getchar(void);
int  readline(char* buf, int max);

/* 文件系统 */
int  fs_open(const char* name);
int  fs_close(int fd);
long fs_read(int fd, void* buf, long n);
long fs_write(int fd, const void* buf, long n);
long fs_ls(char* buf, long max);
int  fs_unlink(const char* name);
int fs_sync(void);

#endif