#ifndef SAVEIRO_TASK_H
#define SAVEIRO_TASK_H

#include <types.h>
#include <isr.h>

#define TASK_MAX        16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_DEAD = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING
} task_state_t;

typedef struct task {
    uint32_t      id;
    char          name[24];
    task_state_t  state;
    uint32_t     *stack_base;
    uint32_t      esp;
    uint32_t      ticks;
    volatile uint32_t work;
    void        (*entry)(void *arg);
    void         *arg;
    struct task  *next;
} task_t;

void     sched_init(void);
int      task_create(const char *name, void (*entry)(void *), void *arg);
int      task_kill(uint32_t id);
uint32_t task_schedule(regs_t *r);
task_t  *task_current(void);
int      task_count(void);
const task_t *task_at(int index);
void     task_update_status(void);
void     sched_lock(void);
void     sched_unlock(void);
void     task_yield(void);
void     task_worker(void *arg);

#endif
