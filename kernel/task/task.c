#include "task.h"
#include "heap.h"
#include "vmm.h"
#include "serial.h"
#include "gdt.h"
#include <stdint.h>

#define TIMESLICE 5
#define MAX_SLEEPERS 32

extern uint64_t kernel_rsp;
extern uint64_t saved_user_rsp;
extern uint64_t syscall_user_rcx;
extern void     fork_child_entry(void);

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
static int       child_alive = 0;
static task_t* foreground = 0;
static wait_queue_t child_exit_wq;

extern void task_switch(uint64_t* old_rsp_ptr, uint64_t new_rsp);

static inline void write_cr3(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(v) : "memory");
}

static inline void set_kernel_rsp(uint64_t v) { kernel_rsp = v; }

void task_init(void) {
    for (int i = 0; i < MAX_SLEEPERS; i++) sleepers[i].used = 0;
    global_tick = 0;

    current = (task_t*)kmalloc(sizeof(task_t));
    current->rsp        = 0;
    current->stack_base = 0;
    current->stack_size = 0;
    current->kernel_stack_top = 0;
    current->saved_user_rsp   = 0;
    current->pml4_phys        = 0;
    current->id         = 0;
    current->state      = TASK_RUNNING;
    current->name       = "kmain";
    current->next       = current;
    current->wq_next    = 0;
    current->parent     = 0;
    wait_queue_init(&child_exit_wq);
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
    t->parent     = 0;
    t->saved_user_rsp = 0;
    t->pml4_phys      = 0;

    uint64_t stack_top = t->stack_base + TASK_STACK_SIZE;
    stack_top &= ~0xFULL;
    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = 0;                       /* dummy */
    *--sp = (uint64_t)entry;         /* ret 目标 */
    *--sp = 0x202;                   /* rflags */
    *--sp = 0;                       /* rbp */
    *--sp = 0;                       /* rbx */
    *--sp = 0;                       /* r12 */
    *--sp = 0;                       /* r13 */
    *--sp = 0;                       /* r14 */
    *--sp = 0;                       /* r15 */
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
    prev->saved_user_rsp = saved_user_rsp;   /* 保存当前任务的 */
    saved_user_rsp = next->saved_user_rsp;   /* 加载下一个任务的 */
    if (next->pml4_phys) {
        write_cr3(next->pml4_phys);
    }

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

int task_fork(void) {
    task_t* parent = current;

    task_t* child = (task_t*)kmalloc(sizeof(task_t));
    if (!child) return -1;

    uint64_t parent_pml4 = vmm_current_pml4();
    uint64_t child_pml4  = vmm_clone_pml4_deep(parent_pml4);
    if (!child_pml4) {
        kfree(child);
        return -1;
    }

    child->stack_base       = (uint64_t)kmalloc(TASK_STACK_SIZE);
    child->stack_size       = TASK_STACK_SIZE;
    child->kernel_stack_top = child->stack_base + TASK_STACK_SIZE;
    child->pml4_phys        = child_pml4;
    child->saved_user_rsp   = saved_user_rsp;
    child->id               = next_id++;
    child->state            = TASK_READY;
    child->name             = "child";
    child->next             = 0;
    child->wq_next          = 0;
    child->parent           = parent;

    uint64_t top = child->kernel_stack_top & ~0xFULL;
    uint64_t* sp = (uint64_t*)top;
    *--sp = (uint64_t)fork_child_entry;
    *--sp = 0x202;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = saved_user_rsp;
    *--sp = syscall_user_rcx;
    child->rsp = (uint64_t)sp;

    child->next   = current->next;
    current->next = child;

    serial_printf("FORK: child id=%u pml4=0x%lx user_rip=0x%lx user_rsp=0x%lx\n",
                  child->id, child_pml4, syscall_user_rcx, saved_user_rsp);

    return (int)child->id;
}

int task_current_id(void) {
    return current ? (int)current->id : -1;
}

void task_set_foreground(task_t* t) { foreground = t; }
task_t* task_get_foreground(void)   { return foreground; }

void task_wait_child(void) {
    task_block(&child_exit_wq);
}

void task_signal_child_exit(void) {
    wait_queue_wake_all(&child_exit_wq);
}
void task_kill_all_children(void) {
    if (!current) return;
    task_t* t = current->next;
    while (t && t != current) {
        if (t->id != 0) t->state = TASK_DEAD;
        t = t->next;
    }
}

int  task_child_count(void) { return child_alive; }
void task_child_inc(void)   { child_alive++; }
void task_child_dec(void)   { if (child_alive > 0) child_alive--; }
