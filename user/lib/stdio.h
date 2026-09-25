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
int  fs_sync(void);
long fs_fat_ls(char* buf, long max);
long fs_fat_read(const char* name, void* buf, long max);
long fs_fat_write(const char* name, const void* data, long size);
long fs_fat_delete(const char* name);

/* 目录 */
int  fs_mkdir(const char* path);
int  fs_chdir(const char* path);
int  fs_getcwd(char* buf, int max);
long fs_ls_path(const char* path, char* buf, long max);

void lock_stdout(void);
void unlock_stdout(void);

#endif