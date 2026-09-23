#ifndef MYOS_TASK_H
#define MYOS_TASK_H

#include <stdint.h>

#define TASK_STACK_SIZE (8 * 1024)

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} task_state_t;

struct task;
typedef struct task task_t;

typedef struct wait_queue {
    task_t* head;
    task_t* tail;
} wait_queue_t;

struct task {
    uint64_t     rsp;
    uint64_t     stack_base;
    uint64_t     stack_size;
    uint64_t     kernel_stack_top;
    uint64_t     saved_user_rsp;
    uint64_t     pml4_phys;         /* 新增：本任务的 PML4 物理地址，0 = 用当前 CR3 */
    uint32_t     id;
    task_state_t state;
    const char*  name;
    task_t*      next;
    task_t*      wq_next;
};

void    task_init(void);
task_t* task_create(const char* name, void (*entry)(void));
void    schedule(void);
void    scheduler_tick(void);
task_t* task_current(void);

void wait_queue_init(wait_queue_t* wq);
void task_block(wait_queue_t* wq);
void wait_queue_wake_one(wait_queue_t* wq);
void wait_queue_wake_all(wait_queue_t* wq);

void task_sleep(uint64_t ms);   /* ← 新增 */

#endif