#include <task.h>
#include <pmm.h>
#include <heap.h>
#include <string.h>
#include <io.h>
#include <vga.h>
#include <pit.h>
#include <console.h>
#include <rtc.h>

static task_t  tasks[TASK_MAX];
static task_t *current;
static task_t *head;
static task_t *tail;
static int     used_tasks;
static int     next_id = 1;
static volatile int sched_locked;

static void task_trampoline(void);

void sched_init(void)
{
    task_t *t = &tasks[0];

    memset(tasks, 0, sizeof(tasks));

    t->id = 0;
    strcpy(t->name, "shell");
    t->state = TASK_RUNNING;
    t->next = t;

    current = head = tail = t;
    used_tasks = 1;
    next_id = 1;
    sched_locked = 0;
}

int task_create(const char *name, void (*entry)(void *), void *arg)
{
    uint32_t stack;
    uint32_t *sp;
    task_t *t;
    task_t *saved_next = NULL;
    int slot = -1;
    int i;

    for (i = 1; i < used_tasks; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            saved_next = tasks[i].next;
            break;
        }
    }

    if (slot < 0) {
        if (used_tasks >= TASK_MAX)
            return -1;
        slot = used_tasks;
    }

    stack = pmm_alloc_page();
    if (!stack)
        return -1;

    t = &tasks[slot];
    memset(t, 0, sizeof(*t));
    t->next = saved_next;

    t->id = (uint32_t)next_id++;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->stack_base = (uint32_t *)stack;
    t->entry = entry;
    t->arg = arg;
    t->state = TASK_READY;

    /* Fabricar el marco que el stub de interrupción espera encontrar:
     * eflags, cs, eip, err, int_no y los 8 registros de pushal. */
    sp = (uint32_t *)(stack + TASK_STACK_SIZE);
    *--sp = 0x202;                       /* eflags con IF=1 */
    *--sp = 0x08;                        /* cs */
    *--sp = (uint32_t)task_trampoline;   /* eip */
    *--sp = 0;                           /* err_code */
    *--sp = 32;                          /* int_no (como si viniera del timer) */
    for (i = 0; i < 8; i++)
        *--sp = 0;

    t->esp = (uint32_t)sp;

    if (slot == used_tasks) {
        tail->next = t;
        t->next = head;
        tail = t;
        used_tasks++;
    }

    return (int)t->id;
}

static void task_trampoline(void)
{
    task_t *t = current;

    if (t->entry)
        t->entry(t->arg);

    t->state = TASK_DEAD;

    for (;;) {
        sti();
        hlt();
    }
}

uint32_t task_schedule(regs_t *r)
{
    task_t *next;
    int i;

    if (!current)
        return (uint32_t)r;

    current->esp = (uint32_t)r;

    if (sched_locked || used_tasks <= 1)
        return (uint32_t)r;

    next = current->next;

    for (i = 0; i < used_tasks; i++) {
        if (next->state != TASK_DEAD)
            break;
        next = next->next;
    }

    if (next->state == TASK_DEAD)
        return (uint32_t)r;

    if (current != next && current->state == TASK_RUNNING)
        current->state = TASK_READY;

    current = next;
    next->state = TASK_RUNNING;
    next->ticks++;

    return next->esp;
}

int task_kill(uint32_t id)
{
    int i;

    if (id == 0)
        return -1;

    for (i = 1; i < used_tasks; i++) {
        if (tasks[i].id == id && tasks[i].state != TASK_DEAD) {
            tasks[i].state = TASK_DEAD;

            if (&tasks[i] != current && tasks[i].stack_base) {
                pmm_free_page((uint32_t)tasks[i].stack_base);
                tasks[i].stack_base = NULL;
            }

            return 0;
        }
    }

    return -1;
}

task_t *task_current(void)
{
    return current;
}

int task_count(void)
{
    int i;
    int alive = 0;

    for (i = 0; i < used_tasks; i++) {
        if (tasks[i].state != TASK_DEAD)
            alive++;
    }

    return alive;
}

const task_t *task_at(int index)
{
    if (index < 0 || index >= used_tasks)
        return NULL;

    return &tasks[index];
}

void task_update_status(void)
{
    char buf[VGA_COLS + 1];
    rtc_time_t now;

    rtc_read(&now);

    ksnprintf(buf, sizeof(buf),
              " SaveiroOS 0.1 | RAM %u/%u MB | heap %u KB | tareas %d | %02u:%02u:%02u | up %us",
              pmm_free_bytes() / (1024u * 1024u), pmm_total_bytes() / (1024u * 1024u),
              heap_used_bytes() / 1024u, task_count(),
              now.hour, now.minute, now.second, timer_uptime());

    con_status(buf);
}

void sched_lock(void)
{
    sched_locked++;
}

void sched_unlock(void)
{
    if (sched_locked > 0)
        sched_locked--;
}

void task_yield(void)
{
    __asm__ volatile("sti; hlt");
}

void task_worker(void *arg)
{
    task_t *t = current;

    UNUSED(arg);

    if (!t)
        return;

    for (;;) {
        t->work++;
        __asm__ volatile("hlt");
    }
}
