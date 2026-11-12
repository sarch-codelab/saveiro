#include <syscall.h>
#include <console.h>
#include <task.h>
#include <pit.h>
#include <string.h>

/* Llamadas al sistema de SaveiroOS (int 0x80)
 *
 *   eax = 1  write(cadena, longitud)
 *   eax = 2  exit(codigo)
 *   eax = 3  getpid()
 *   eax = 4  uptime()
 */
void syscall_handle(regs_t *r)
{
    switch (r->eax) {
    case 1:
        if (r->ebx && r->ecx <= 4096)
            kwrite((const char *)r->ebx, r->ecx);
        r->eax = r->ecx;
        break;

    case 2:
        kprintf("[syscall] exit(%u) solicitado por la tarea %u\n",
                r->ebx, task_current() ? task_current()->id : 0);
        r->eax = 0;
        break;

    case 3:
        r->eax = task_current() ? task_current()->id : 0;
        break;

    case 4:
        r->eax = timer_uptime();
        break;

    default:
        r->eax = (uint32_t)-1;
        break;
    }
}
