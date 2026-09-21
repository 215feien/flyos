#ifndef MYOS_PERSIST_H
#define MYOS_PERSIST_H

#include <stdint.h>

#define PERSIST_START_LBA   2048        /* 从 1 MB 处开始 */
#define PERSIST_MAX_SECTORS 1024        

int persist_load(void);
int persist_save(void);

#endif