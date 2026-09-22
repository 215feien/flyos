#ifndef MYOS_SEM_H
#define MYOS_SEM_H

#include <stdint.h>

#define SEM_MAX 16

void sem_init_all(void);
void sem_init(int id, int count);
void sem_wait(int id);
void sem_post(int id);
int  sem_value(int id);

#endif