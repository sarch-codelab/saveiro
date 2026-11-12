#ifndef SAVEIRO_PIT_H
#define SAVEIRO_PIT_H

#include <types.h>

#define PIT_HZ 100

void pit_init(void);
void timer_irq(void);
uint32_t timer_ticks(void);
uint32_t timer_uptime(void);
void timer_sleep_ms(uint32_t ms);

#endif
