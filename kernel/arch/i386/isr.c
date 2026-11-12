#include <isr.h>
#include <console.h>
#include <io.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>
#include <task.h>
#include <syscall.h>

static const char *exception_names[32] = {
    "division por cero",
    "depuracion",
    "NMI",
    "breakpoint",
    "desbordamiento",
    "limite de BOUND",
    "instruccion invalida",
    "dispositivo no disponible",
    "doble falta",
    "segmento de coprocesador",
    "TSS invalido",
    "segmento no presente",
    "falta de pila",
    "falta de proteccion general",
    "falta de pagina",
    "reservada",
    "error de coma flotante",
    "error de alineacion",
    "fallo de maquina",
    "SIMD",
    "virtualizacion",
    "control de proteccion",
    "reservada",
    "reservada",
    "reservada",
    "reservada",
    "reservada",
    "reservada",
    "hipervisor",
    "control de virtualizacion",
    "error de seguridad",
    "reservada"
};

static void dump_registers(regs_t *r)
{
    kprintf("  eax=%08x ebx=%08x ecx=%08x edx=%08x\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf("  esi=%08x edi=%08x ebp=%08x esp=%08x\n", r->esi, r->edi, r->ebp, r->esp);
    kprintf("  eip=%08x cs=%04x eflags=%08x err=%08x\n", r->eip, r->cs, r->eflags, r->err_code);
}

void panic(const char *reason, regs_t *r)
{
    cli();
    kprintf("\n");
    kprintf("*** PANIC DEL KERNEL ***\n");
    kprintf("  %s\n", reason);
    if (r)
        dump_registers(r);
    kprintf("  el sistema queda detenido.\n");

    for (;;)
        hlt();
}

static void exception(regs_t *r)
{
    const char *name = "excepcion desconocida";

    if (r->int_no < 32)
        name = exception_names[r->int_no];

    kprintf("\n*** EXCEPCION %u: %s ***\n", r->int_no, name);
    if (r->int_no == 14) {
        uint32_t cr2;

        __asm__ volatile("movl %%cr2, %0" : "=r"(cr2));
        kprintf("  direccion que falló: 0x%08x\n", cr2);
    }
    panic("excepcion de la CPU", r);
}

uint32_t isr_dispatch(regs_t *r)
{
    if (r->int_no < 32)
        exception(r);

    if (r->int_no >= 32 && r->int_no < 48) {
        uint8_t irq = (uint8_t)(r->int_no - 32);

        if (irq == 0) {
            timer_irq();
            pic_eoi(0);
            return task_schedule(r);
        }

        if (irq == 1)
            keyboard_irq();

        if (irq != 7)
            pic_eoi(irq);

        return (uint32_t)r;
    }

    if (r->int_no == 0x80) {
        syscall_handle(r);
        return (uint32_t)r;
    }

    return (uint32_t)r;
}
