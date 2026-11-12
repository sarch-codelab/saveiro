#include <pit.h>
#include <io.h>
#include <task.h>

#define PIT_BASE_HZ 1193182u

static volatile uint32_t ticks;
static volatile uint32_t seconds;

void pit_init(void)
{
    uint32_t divisor = PIT_BASE_HZ / PIT_HZ;

    outb(0x43, 0x36);                       /* canal 0, modo 3, binario */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_irq(void)
{
    ticks++;

    if (ticks % PIT_HZ == 0)
        seconds++;

    if (ticks % 50 == 0)
        task_update_status();
}

uint32_t timer_ticks(void)
{
    return ticks;
}

uint32_t timer_uptime(void)
{
    return seconds;
}

void timer_sleep_ms(uint32_t ms)
{
    uint32_t target = ticks + (ms * PIT_HZ) / 1000;

    while (ticks < target)
        __asm__ volatile("sti; hlt");
}
