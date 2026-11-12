#include <types.h>
#include <bootinfo.h>
#include <console.h>
#include <vga.h>
#include <serial.h>
#include <io.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>
#include <pmm.h>
#include <paging.h>
#include <heap.h>
#include <ramfs.h>
#include <rtc.h>
#include <task.h>
#include <shell.h>

static void log_ok(const char *text)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kputs("[ ok ] ");
    vga_reset_color();
    kprintf("%s\n", text);
}

void kmain(boot_info_t *bi)
{
    vga_init();
    console_init(vga_console());
    serial_init();

    print_banner();

    log_ok("modo protegido de 32 bits, pila del kernel en 0x90000");

    gdt_init();
    log_ok("GDT propia: segmentos planos de 4 GiB (0x08 codigo, 0x10 datos)");

    idt_init();
    log_ok("IDT con 48 vectores de interrupcion y puerta int 0x80");

    pic_init();
    pit_init();
    keyboard_init();
    log_ok("PIC remapeado a 0x20/0x28, PIT programado a 100 Hz, teclado PS/2");

    pmm_init(bi);
    kprintf("[ ok ] memoria detectada por el BIOS (E820): %u MiB utilizables\n",
            pmm_total_bytes() / (1024u * 1024u));

    paging_init();
    log_ok("paginacion activada: PSE con paginas de 4 MiB, identidad 0..128 MiB");

    heap_init();
    log_ok("heap del kernel: 4 MiB en 0x00400000");

    ramfs_init();
    kprintf("[ ok ] RAMFS montado con %d archivos en memoria\n", ramfs_count());

    sched_init();
    log_ok("planificador round-robin listo (tarea 0 = shell)");

    pic_unmask(0);
    pic_unmask(1);
    sti();
    log_ok("interrupciones activadas; el temporizador ya corre");

    kprintf("\nArranque completo. Bienvenido.\n");

    shell_run();
}
