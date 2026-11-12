#ifndef SAVEIRO_ISR_H
#define SAVEIRO_ISR_H

#include <types.h>

/* Marco que el stub en ensamblador deja en la pila. */
typedef struct regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} regs_t;

void isr_init(void);
void panic(const char *reason, regs_t *r);

#endif
