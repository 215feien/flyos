#include "sem.h"
#include "task.h"
#include "serial.h"

typedef struct {
    int count;
    int used;
    wait_queue_t wq;
} sem_t;

static sem_t sems[SEM_MAX];

void sem_init_all(void) {
    for (int i = 0; i < SEM_MAX; i++) {
        sems[i].count = 0;
        sems[i].used = 0;
        wait_queue_init(&sems[i].wq);
    }
}

void sem_init(int id, int count) {
    if (id < 0 || id >= SEM_MAX) return;
    sems[id].count = count;
    sems[id].used = 1;
    wait_queue_init(&sems[id].wq);
}

void sem_wait(int id) {
    if (id < 0 || id >= SEM_MAX || !sems[id].used) return;

    sems[id].count--;
    if (sems[id].count < 0) {
        task_block(&sems[id].wq);
    }
}

void sem_post(int id) {
    if (id < 0 || id >= SEM_MAX || !sems[id].used) return;

    sems[id].count++;
    if (sems[id].count <= 0) {
        wait_queue_wake_one(&sems[id].wq);
    }
}

int sem_value(int id) {
    if (id < 0 || id >= SEM_MAX || !sems[id].used) return -1;
    return sems[id].count;
}