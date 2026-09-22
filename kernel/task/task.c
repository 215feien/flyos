#include "task.h"
#include "heap.h"
#include "serial.h"
#include "gdt.h"
#include <stdint.h>

extern uint64_t saved_user_rsp;
extern uint64_t kernel_rsp;

static uint64_t get_saved_user_rsp(void) { return saved_user_rsp; }
static void     set_saved_user_rsp(uint64_t v) { saved_user_rsp = v; }

extern uint64_t kernel_rsp;
static inline void set_kernel_rsp(uint64_t v) { kernel_rsp = v; }

#define TIMESLICE 5
#define MAX_SLEEPERS 32

static task_t* current = 0;
static uint32_t next_id = 1;
static uint32_t tick_in_slice = 0;

typedef struct {
    task_t*  task;
    uint64_t wake_at;
    int      used;
} sleeper_t;

static sleeper_t sleepers[MAX_SLEEPERS];
static uint64_t  global_tick = 0;

extern void task_switch(uint64_t* old_rsp_ptr, uint64_t new_rsp);

void task_init(void) {
    for (int i = 0; i < MAX_SLEEPERS; i++) sleepers[i].used = 0;
    global_tick = 0;

    current = (task_t*)kmalloc(sizeof(task_t));
    current->rsp        = 0;
    current->stack_base = 0;
    current->stack_size = 0;
    current->kernel_stack_top = 0;
    current->id         = 0;
    current->state      = TASK_RUNNING;
    current->name       = "kmain";
    current->next       = current;
    current->wq_next    = 0;
    current->saved_user_rsp = 0;
    serial_printf("TASK: init, current = kmain\n");
}

task_t* task_create(const char* name, void (*entry)(void)) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    t->stack_base = (uint64_t)kmalloc(TASK_STACK_SIZE);
    t->stack_size = TASK_STACK_SIZE;
    t->id         = next_id++;
    t->state      = TASK_READY;
    t->name       = name;
    t->wq_next    = 0;
    t->saved_user_rsp = 0;

    uint64_t stack_top = t->stack_base + TASK_STACK_SIZE;
    stack_top &= ~0xFULL;
    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = 0;
    *--sp = (uint64_t)entry;
    *--sp = 0x202ULL;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    t->rsp = (uint64_t)sp;
    t->kernel_stack_top = t->stack_base + TASK_STACK_SIZE;

    t->next = current->next;
    current->next = t;

    serial_printf("TASK: created '%s' id=%u rsp=0x%lx entry=0x%lx\n",
                  t->name, t->id, t->rsp, (uint64_t)entry);
    return t;
}

void schedule(void) {
    if (!current) return;
    task_t* prev = current;
    task_t* next = current;

    do {
        next = next->next;
        if (next->state == TASK_READY) break;
    } while (next != current);

    if (next == current) return;

    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current = next;

    if (next->kernel_stack_top) {
        tss_set_rsp0(next->kernel_stack_top);
        set_kernel_rsp(next->kernel_stack_top);
    }

    /* 保存当前任务的用户态 rsp，加载下一个任务的 */
    prev->saved_user_rsp = get_saved_user_rsp();
    set_saved_user_rsp(next->saved_user_rsp);

    task_switch(&prev->rsp, next->rsp);
}

void scheduler_tick(void) {
    global_tick++;

    for (int i = 0; i < MAX_SLEEPERS; i++) {
        if (sleepers[i].used && sleepers[i].wake_at <= global_tick) {
            if (sleepers[i].task->state == TASK_BLOCKED) {
                sleepers[i].task->state = TASK_READY;
            }
            sleepers[i].used = 0;
        }
    }

    tick_in_slice++;
    if (tick_in_slice >= TIMESLICE) {
        tick_in_slice = 0;
        schedule();
    }
}

task_t* task_current(void) { return current; }

void wait_queue_init(wait_queue_t* wq) {
    wq->head = 0;
    wq->tail = 0;
}

void task_block(wait_queue_t* wq) {
    if (!current) return;
    current->state = TASK_BLOCKED;
    current->wq_next = 0;

    if (wq->tail) {
        wq->tail->wq_next = current;
    } else {
        wq->head = current;
    }
    wq->tail = current;

    schedule();
}

void wait_queue_wake_one(wait_queue_t* wq) {
    if (!wq->head) return;
    task_t* t = wq->head;
    wq->head = t->wq_next;
    if (!wq->head) wq->tail = 0;
    t->wq_next = 0;
    if (t->state == TASK_BLOCKED) t->state = TASK_READY;
}

void wait_queue_wake_all(wait_queue_t* wq) {
    while (wq->head) wait_queue_wake_one(wq);
}

void task_sleep(uint64_t ms) {
    uint64_t ticks = (ms + 9) / 10;
    if (ticks == 0) ticks = 1;

    int slot = -1;
    for (int i = 0; i < MAX_SLEEPERS; i++) {
        if (!sleepers[i].used) { slot = i; break; }
    }
    if (slot < 0) return;

    sleepers[slot].task    = current;
    sleepers[slot].wake_at = global_tick + ticks;
    sleepers[slot].used    = 1;

    current->state = TASK_BLOCKED;
    schedule();
}
