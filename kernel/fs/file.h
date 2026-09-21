#ifndef MYOS_FILE_H
#define MYOS_FILE_H

#include <stdint.h>

void    file_init(void);
int     file_open(const char* name);
int     file_close(int fd);
int64_t file_read (int fd, void* buf, uint64_t n);
int64_t file_write(int fd, const void* buf, uint64_t n);

#endif