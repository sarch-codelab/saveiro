#include <shell.h>
#include <types.h>
#include <console.h>
#include <vga.h>
#include <keyboard.h>
#include <string.h>
#include <pmm.h>
#include <paging.h>
#include <heap.h>
#include <ramfs.h>
#include <task.h>
#include <pit.h>
#include <rtc.h>
#include <io.h>
#include <gfx.h>
#include <gui.h>
#include <mouse.h>

#define LINE_MAX 128

typedef struct {
    const char *name;
    const char *help;
    void (*fn)(int argc, char **argv);
} command_t;

static void cmd_help(int argc, char **argv);
static void cmd_about(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_ps(int argc, char **argv);
static void cmd_spawn(int argc, char **argv);
static void cmd_kill(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_write(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_date(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_ticks(int argc, char **argv);
static void cmd_syscall(int argc, char **argv);
static void cmd_test(int argc, char **argv);
static void cmd_reboot(int argc, char **argv);
static void cmd_halt(int argc, char **argv);
static void cmd_gui(int argc, char **argv);
static void cmd_mouse(int argc, char **argv);

static const command_t commands[] = {
    { "help",    "muestra esta ayuda",                    cmd_help },
    { "about",   "informacion del sistema",               cmd_about },
    { "logo",    "vuelve a dibujar el logotipo",          cmd_about },
    { "clear",   "limpia la pantalla",                    cmd_clear },
    { "echo",    "escribe el texto dado",                 cmd_echo },
    { "mem",     "memoria fisica, heap y paginacion",     cmd_mem },
    { "ps",      "lista las tareas del planificador",     cmd_ps },
    { "spawn",   "crea tareas de prueba: spawn [n]",      cmd_spawn },
    { "kill",    "termina una tarea: kill ID",            cmd_kill },
    { "ls",      "lista los archivos del RAMFS",          cmd_ls },
    { "cat",     "muestra un archivo: cat NOMBRE",        cmd_cat },
    { "write",   "escribe en un archivo: write F texto",  cmd_write },
    { "rm",      "borra un archivo: rm NOMBRE",           cmd_rm },
    { "date",    "fecha y hora del reloj CMOS",           cmd_date },
    { "uptime",  "segundos desde el arranque",            cmd_uptime },
    { "ticks",   "ticks del temporizador (100 Hz)",       cmd_ticks },
    { "syscall", "prueba las llamadas int 0x80",          cmd_syscall },
    { "test",    "ejecuta las autopruebas del kernel",    cmd_test },
    { "reboot",  "reinicia la maquina",                   cmd_reboot },
    { "halt",    "detiene el sistema",                    cmd_halt },
    { "gui",     "abre el escritorio grafico 800x600",    cmd_gui },
    { "mouse",   "estado del raton PS/2",                 cmd_mouse },
};

#define COMMAND_COUNT ((int)(sizeof(commands) / sizeof(commands[0])))

void print_banner(void)
{
    con_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\n");
    kprintf("  ____                   _                ___  ____  \n");
    kprintf(" / ___|  __ ___   _____ (_) _ __ ___    / _ \\/ ___| \n");
    kprintf(" \\___ \\ / _` \\ \\ / / _ \\| || '__/ _ \\  | | | \\___ \\ \n");
    kprintf("  ___) | (_| |\\ V /  __/| || | | (_) | | |_| |___) |\n");
    kprintf(" |____/ \\__,_| \\_/ \\___||_||_|  \\___/   \\___/|____/ \n");
    con_set_color(VGA_WHITE, VGA_BLACK);
    kprintf("        SaveiroOS 0.1  -  i386  -  desde cero\n\n");
    con_reset_color();
}

static void prompt(void)
{
    con_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kputs("saveiro");
    con_set_color(VGA_DARK_GREY, VGA_BLACK);
    kputs(":/");
    con_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("$ ");
    con_reset_color();
}

static int read_line(char *buffer, int cap)
{
    int length = 0;

    for (;;) {
        int c = input_getchar();

        if (c == '\n') {
            kputc('\n');
            buffer[length] = '\0';
            return length;
        }

        if (c == '\b' || c == 127) {
            if (length > 0) {
                length--;
                kputs("\b \b");
            }
            continue;
        }

        if (c >= 32 && c < 127 && length < cap - 1) {
            buffer[length++] = (char)c;
            kputc((char)c);
        }
    }
}

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (argc >= max)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }

    return argc;
}

static void cmd_help(int argc, char **argv)
{
    int i;

    UNUSED(argc);
    UNUSED(argv);

    con_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\nComandos disponibles\n");
    con_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("--------------------\n");
    con_reset_color();

    for (i = 0; i < COMMAND_COUNT; i++) {
        con_set_color(VGA_YELLOW, VGA_BLACK);
        kprintf("  %-9s", commands[i].name);
        con_reset_color();
        kprintf(" %s\n", commands[i].help);
    }
    kprintf("\n");
}

static void cmd_about(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    print_banner();
    kprintf("  Version    : 0.1\n");
    kprintf("  Arquitectura: x86 (i386), 32 bits, modo protegido\n");
    kprintf("  Arranque   : bootloader propio de dos etapas, disquete de 1.44 MB\n");
    kprintf("  Memoria    : %u MiB detectados, paginacion PSE de 4 MiB\n",
            pmm_total_bytes() / (1024u * 1024u));
    kprintf("  Procesos   : planificador round-robin a 100 Hz\n");
    kprintf("  Archivos   : RAMFS en memoria\n");
    kprintf("  Emulador   : v86 (WebAssembly) en tu navegador\n\n");
}

static void cmd_echo(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (i > 1)
            kputc(' ');
        kputs(argv[i]);
    }
    kputc('\n');
}

static void cmd_clear(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    con_clear();
    task_update_status();
}

static void cmd_mem(int argc, char **argv)
{
    uint32_t total = pmm_total_bytes();
    uint32_t freeb = pmm_free_bytes();

    UNUSED(argc);
    UNUSED(argv);

    kprintf("\nMemoria fisica\n");
    kprintf("  total detectado : %u MiB\n", total / (1024u * 1024u));
    kprintf("  libre           : %u MiB\n", freeb / (1024u * 1024u));
    kprintf("  en uso          : %u MiB\n", (total - freeb) / (1024u * 1024u));
    kprintf("  paginas         : %u totales, %u usadas, %u libres\n",
            pmm_total_pages(), pmm_used_pages(), pmm_total_pages() - pmm_used_pages());

    kprintf("\nHeap del kernel\n");
    kprintf("  base            : 0x%08x\n", HEAP_START);
    kprintf("  tamano          : %u KiB\n", heap_total_bytes() / 1024u);
    kprintf("  en uso          : %u bytes\n", heap_used_bytes());
    kprintf("  libre           : %u KiB\n", heap_free_bytes() / 1024u);

    kprintf("\nPaginacion\n");
    kprintf("  directorio      : 0x%08x\n", paging_directory_phys());
    kprintf("  modo            : PSE, paginas de 4 MiB, identidad 0..128 MiB\n\n");
}

static void cmd_ps(int argc, char **argv)
{
    int i;

    UNUSED(argc);
    UNUSED(argv);

    con_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\n  ID  NOMBRE           ESTADO    TICKS   TRABAJO\n");
    con_reset_color();

    for (i = 0; i < TASK_MAX; i++) {
        const task_t *t = task_at(i);
        const char *state;

        if (!t || t->state == TASK_DEAD)
            continue;

        state = "?";
        switch (t->state) {
        case TASK_READY:   state = "listo";     break;
        case TASK_RUNNING: state = "ejecutando"; break;
        case TASK_SLEEPING: state = "dormido";  break;
        default: break;
        }

        kprintf("  %2u  %-16s %-9s %6u  %8u\n",
                t->id, t->name, state, t->ticks, t->work);
    }
    kprintf("\n");
}

static void cmd_spawn(int argc, char **argv)
{
    int count = argc > 1 ? atoi(argv[1]) : 1;
    int created = 0;
    int i;

    if (count < 1)
        count = 1;
    if (count > TASK_MAX - 1)
        count = TASK_MAX - 1;

    for (i = 0; i < count; i++) {
        char name[24];
        int id;

        ksnprintf(name, sizeof(name), "worker-%d", i + 1);
        id = task_create(name, task_worker, NULL);
        if (id < 0)
            break;
        created++;
    }

    if (created)
        kprintf("creadas %d tarea(s) de prueba; mira el contador con 'ps'\n", created);
    else
        kprintf("no se pudo crear ninguna tarea\n");
}

static void cmd_kill(int argc, char **argv)
{
    int id;

    if (argc < 2) {
        kprintf("uso: kill ID\n");
        return;
    }

    id = atoi(argv[1]);

    if (task_kill((uint32_t)id) == 0)
        kprintf("tarea %d terminada\n", id);
    else
        kprintf("no existe la tarea %d (o es el shell)\n", id);
}

static void cmd_ls(int argc, char **argv)
{
    int i;
    int count = ramfs_count();

    UNUSED(argc);
    UNUSED(argv);

    if (count == 0) {
        kprintf("(RAMFS vacio)\n");
        return;
    }

    kprintf("\n  TAMANO  NOMBRE\n");
    for (i = 0; i < count; i++) {
        kprintf("  %6u  %s\n", ramfs_size_at(i), ramfs_name_at(i));
    }
    kprintf("\n");
}

static void cmd_cat(int argc, char **argv)
{
    char *buffer;
    int n;

    if (argc < 2) {
        kprintf("uso: cat NOMBRE\n");
        return;
    }

    buffer = (char *)kmalloc(RAMFS_MAX_SIZE + 1);
    if (!buffer) {
        kprintf("sin memoria\n");
        return;
    }

    n = ramfs_read(argv[1], buffer, RAMFS_MAX_SIZE);
    if (n < 0) {
        kprintf("no existe el archivo '%s'\n", argv[1]);
        kfree(buffer);
        return;
    }

    buffer[n] = '\0';
    kwrite(buffer, (size_t)n);
    if (n == 0 || buffer[n - 1] != '\n')
        kputc('\n');

    kfree(buffer);
}

static void cmd_write(int argc, char **argv)
{
    char text[256];
    int length = 0;
    int i;

    if (argc < 3) {
        kprintf("uso: write NOMBRE texto\n");
        return;
    }

    if (!ramfs_exists(argv[1]))
        ramfs_create(argv[1]);

    text[0] = '\0';
    for (i = 2; i < argc; i++) {
        int n = (int)strlen(argv[i]);

        if (length + n + 2 >= (int)sizeof(text))
            break;
        if (length > 0)
            text[length++] = ' ';
        memcpy(text + length, argv[i], (size_t)n);
        length += n;
    }
    text[length++] = '\n';

    if (ramfs_write(argv[1], text, (size_t)length, true) < 0)
        kprintf("no se pudo escribir (archivo demasiado grande)\n");
    else
        kprintf("%d bytes anadidos a %s\n", length, argv[1]);
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("uso: rm NOMBRE\n");
        return;
    }

    if (ramfs_unlink(argv[1]) == 0)
        kprintf("'%s' borrado\n", argv[1]);
    else
        kprintf("no existe el archivo '%s'\n", argv[1]);
}

static void cmd_date(int argc, char **argv)
{
    rtc_time_t now;

    UNUSED(argc);
    UNUSED(argv);

    rtc_read(&now);
    kprintf("20%02u-%02u-%02u %02u:%02u:%02u UTC (reloj CMOS)\n",
            now.year, now.month, now.day, now.hour, now.minute, now.second);
}

static void cmd_uptime(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    kprintf("uptime: %u segundos\n", timer_uptime());
}

static void cmd_ticks(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    kprintf("ticks: %u (PIT a %u Hz)\n", timer_ticks(), PIT_HZ);
}

static void cmd_syscall(int argc, char **argv)
{
    static const char message[] = "hola desde int 0x80\n";
    uint32_t result;

    UNUSED(argc);
    UNUSED(argv);

    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "a"(1), "b"((uint32_t)message), "c"((uint32_t)(sizeof(message) - 1))
                     : "memory");
    kprintf("[syscall] write() devolvio %u\n", result);

    __asm__ volatile("int $0x80" : "=a"(result) : "a"(3));
    kprintf("[syscall] getpid() devolvio %u\n", result);

    __asm__ volatile("int $0x80" : "=a"(result) : "a"(4));
    kprintf("[syscall] uptime() devolvio %u\n", result);
}

static int tests_pass;
static int tests_fail;

static void check(bool condition, const char *name)
{
    if (condition) {
        tests_pass++;
        con_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kputs("  [ ok ] ");
        con_reset_color();
        kprintf("%s\n", name);
    } else {
        tests_fail++;
        con_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kputs("  [fallo] ");
        con_reset_color();
        kprintf("%s\n", name);
    }
}

static void cmd_test(int argc, char **argv)
{
    uint32_t page_a;
    uint32_t page_b;
    char *block_a;
    char *block_b;
    char buffer[64];
    int n;

    UNUSED(argc);
    UNUSED(argv);

    tests_pass = 0;
    tests_fail = 0;

    kprintf("\nAutopruebas de SaveiroOS\n");
    kprintf("------------------------\n");

    /* Memoria fisica */
    page_a = pmm_alloc_page();
    page_b = pmm_alloc_page();
    check(page_a != 0 && page_b != 0 && page_a != page_b, "PMM entrega paginas distintas");
    check((page_a & 0xFFF) == 0 && (page_b & 0xFFF) == 0, "las paginas estan alineadas");
    if (page_a && page_b) {
        volatile uint32_t *pa = (uint32_t *)page_a;
        volatile uint32_t *pb = (uint32_t *)page_b;

        *pa = 0xDEADBEEF;
        *pb = 0x12345678;
        check(*pa == 0xDEADBEEF && *pb == 0x12345678, "escritura y lectura en RAM");
        pmm_free_page(page_a);
        pmm_free_page(page_b);
        check(pmm_free_bytes() > 0, "PMM libera paginas");
    }

    /* Heap */
    block_a = (char *)kmalloc(64);
    block_b = (char *)kmalloc(1000);
    check(block_a != NULL && block_b != NULL && block_a != block_b, "kmalloc reserva bloques");
    if (block_a && block_b) {
        strcpy(block_a, "saveiro");
        memset(block_b, 0x5A, 1000);
        check(strcmp(block_a, "saveiro") == 0 && block_b[999] == 0x5A, "memoria del heap usable");
        kfree(block_a);
        kfree(block_b);
        check(heap_free_bytes() > 0, "kfree devuelve memoria");
    }

    /* Libreria */
    check(strlen("saveiro") == 7, "strlen");
    check(strcmp("abc", "abc") == 0, "strcmp");
    memset(buffer, 'x', sizeof(buffer));
    memcpy(buffer, "hola", 5);
    check(memcmp(buffer, "hola", 4) == 0 && buffer[4] == 0, "memcpy/memcmp");

    /* RAMFS */
    ramfs_unlink("test.tmp");
    check(ramfs_create("test.tmp") == 0, "RAMFS crea archivos");
    check(ramfs_write("test.tmp", "12345", 5, false) == 5, "RAMFS escribe");
    n = ramfs_read("test.tmp", buffer, sizeof(buffer) - 1);
    if (n >= 0)
        buffer[n] = '\0';
    check(n == 5 && strcmp(buffer, "12345") == 0, "RAMFS lee lo escrito");
    check(ramfs_unlink("test.tmp") == 0, "RAMFS borra archivos");

    /* Interrupciones y temporizador */
    check(timer_ticks() > 0, "el temporizador genera interrupciones");
    check(timer_uptime() < 100000, "el reloj de uptime avanza");

    /* Planificador */
    check(task_count() >= 1, "el planificador ve la tarea actual");

    kprintf("\nRESULTADO: %d/%d pruebas OK\n",
            tests_pass, tests_pass + tests_fail);
    kprintf("%s\n\n", tests_fail == 0 ? "PASS" : "FAIL");
}

static void cmd_reboot(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    kprintf("reiniciando...\n");
    timer_sleep_ms(50);

    outb(0x64, 0xFE);

    for (;;)
        hlt();
}

static void cmd_gui(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    if (gui_is_active()) {
        kprintf("gui: el escritorio ya esta en marcha\n");
        return;
    }

    if (gfx_init(800, 600) < 0) {
        kprintf("gui: el adaptador no soporta el modo 800x600x8 lineal\n");
        return;
    }

    gui_start();

    kprintf("Escritorio iniciado. Mueve el raton y usa el teclado.\n");
    kprintf("ESC vuelve al modo texto.\n");
    con_reset_color();

    /* La sesion grafica continua en este hilo: la shell usa el terminal
     * de la ventana mientras se muestra el escritorio. */
    shell_run();
}


static void cmd_mouse(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    kprintf("raton: %s, paquetes: %u\n",
            mouse_detected() ? "detectado" : "sin respuesta",
            mouse_packets());
}

static void cmd_halt(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    kprintf("sistema detenido. recarga la pagina para arrancar de nuevo.\n");
    cli();

    for (;;)
        hlt();
}

static void execute(char *line)
{
    char *argv[10];
    int argc;
    int i;

    argc = tokenize(line, argv, 10);

    if (argc == 0)
        return;

    for (i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    kprintf("comando no encontrado: %s (prueba 'help')\n", argv[0]);
}

void shell_run(void)
{
    char line[LINE_MAX];

    con_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("Escribe 'help' para ver los comandos.\n");
    con_reset_color();

    for (;;) {
        prompt();
        read_line(line, sizeof(line));
        execute(line);
        task_update_status();
    }
}
