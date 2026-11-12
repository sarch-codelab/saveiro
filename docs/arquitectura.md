# Arquitectura de SaveiroOS

Documento de diseño: cómo arranca la máquina, cómo se reparte la memoria, cómo se
despachan las interrupciones y cómo se cambia de tarea.

---

## 1. Cadena de arranque

```
reset del CPU (0xFFFFFFF0)
  │
  ├─ BIOS / SeaBIOS: inicializa hardware, elige la unidad de arranque
  │
  ├─ carga el sector 0 del disquete en 0x7C00 (DL = unidad)
  │
  ├─ boot/boot.S  (etapa 1, 512 bytes, 16 bits)
  │    · pila en 0x7C00, DS=ES=SS=0
  │    · int 13h AH=02: lee 4 sectores desde el sector 2 hacia 0x7E00
  │    · salto lejano 0000:7E00
  │
  ├─ boot/stage2.S (etapa 2, ~600 bytes, 16 bits)
  │    · A20 por el puerto 0x92
  │    · int 15h E820 → mapa de memoria en 0x5000
  │    · carga el kernel sector a sector (LBA→CHS) en 0x10000
  │    · lgdt + CR0.PE + salto lejano a 32 bits
  │    · pila en 0x90000, push del puntero a boot_info, call 0x10000
  │
  └─ kernel/arch/i386/entry.S
       · guarda el puntero a boot_info, monta la pila, limpia .bss
       · llama a kmain(boot_info)
```

La etapa 2 lleva un marcador `SVRSECTS\0` seguido de un `uint16` con el número de
sectores del kernel. `tools/mkimage.py` parchea ese valor y compone la imagen de 1,44 MB.

---

## 2. Secuencia de `kmain`

```
vga_init / serial_init        consola VGA 80×25 y COM1 a 115200
gdt_init                      GDT propia: null, código 0x08, datos 0x10
idt_init                      256 puertas; 0..47 y 0x80 activas
pic_init                      PIC maestro/esclavo en 0x20/0x28
pit_init                      canal 0, modo 3, 100 Hz
keyboard_init                 vacía el buffer del 8042
pmm_init(boot_info)           bitmap de marcos a partir de E820
paging_init                   PSE, identidad 0..128 MiB
heap_init                     first-fit de 4 MiB en 0x400000
ramfs_init                    siembra README, motd, notas
sched_init                    tarea 0 = shell
gui_boot                      gfx_init + terminal + ratón + wm_start
sti + unmask(IRQ0, IRQ1, IRQ12)
wm_task                       escritorio 800x600, terminal y resto de tareas
shell_run                     bucle prompt → leer línea → ejecutar (dentro del terminal)
```

---

## 3. Mapa de memoria

| Física | Tamaño | Contenido |
|---|---|---|
| `0x00000000`–`0x00000FFF` | 4 KiB | IVT/BDA del BIOS |
| `0x00005000`–`0x0000508F` | — | `boot_info_t`: contador + entradas E820 |
| `0x00007C00` | 512 B | sector de arranque mientras corre |
| `0x00007E00` | 2 KiB | segunda etapa |
| `0x00010000`–`~0x0001C000` | ~48 KiB | kernel (`.text`, `.rodata`, `.data`, `.bss`) |
| `0x00006000`–`0x00006800` | 2 KiB | fuente 8×8 que copia la etapa 2 desde el BIOS |
| `0x00080000`–`0x00090000` | 64 KiB | pila del kernel |
| `0x000B8000` | 4 KiB | texto VGA |
| `0x00100000`–`0x02000000` | 31 MiB | marcos libres (PMM) |
| `0x00400000`–`0x00800000` | 4 MiB | heap del kernel (incluye el *back buffer* de 480 000 B) |
| `0xE0000000` | 480 000 B | framebuffer lineal SVGA (BAR0, mapeado al arrancar) |

`pmm_init` marca todo como usado, libera las regiones `type == 1` del mapa E820 y vuelve
a reservar: primer MiB, `boot_info`, el kernel hasta `_kernel_end` y el heap.

---

## 4. Interrupciones

`kernel/arch/i386/isr_stubs.S` genera 49 stubs con macros:

```
isrN:  cli
       push $0        ; código de error falso, o el real para 8/10/11/12/13/14/17/30
       push $N
       jmp isr_common

isr_common:
       pushal
       mov %esp, %eax
       push %eax
       call isr_dispatch      ; C
       add $4, %esp
       mov %eax, %esp         ; permite cambiar de tarea
       popal
       add $8, %esp           ; quita int_no y err_code
       iret
```

`isr_dispatch`:

1. vectores `< 32` → imprime el nombre de la excepción, `CR2` si es `#PF`, y detiene el sistema;
2. `32..47` → IRQ: temporizador (0), teclado (1), EOI al PIC; el IRQ0 devuelve la pila de la
   siguiente tarea;
3. `0x80` → syscalls.

El EOI se envía antes de cambiar de pila para no perder interrupciones.

---

## 5. Paginación

Se usa **PSE** (Page Size Extension): un único directorio de páginas con 32 entradas de
4 MiB que mapean identidad los primeros 128 MiB. No hay tablas de segundo nivel.

```c
page_directory[i] = i * 0x400000 | 0x83;  // P | RW | PS
CR3 = directorio; CR4.PSE = 1; CR0.PG | CR0.WP = 1;
```

Ventajas: protección de escritura real (`CR0.WP`) y la base para mapear regiones
distintas más adelante. Límite: no hay memoria virtual por proceso.

---

## 6. Heap

Lista enlazada de bloques dentro de `0x400000..0x800000`:

```c
typedef struct block {
    uint32_t magic;   // usado / libre
    uint32_t size;    // bytes útiles
    struct block *next;
    bool free;
} block_t;
```

`kmalloc` recorre la lista, parte el bloque si sobran más de `sizeof(block_t) + 16` bytes
y `kfree` fusiona con el siguiente si también está libre. Los punteros son físicos porque
el mapeo es identidad.

---

## 7. Tareas y cambio de contexto

Estructura clave: `esp` guarda la pila del kernel **en el punto exacto** en que la tarea
quedó interrumpida, es decir, apuntando al marco que dejó `pushal`.

Al crear una tarea se fabrica ese marco a mano:

```
        tope de la página de pila
        ┌──────────────┐
        │ eflags = 0x202│
        │ cs     = 0x08 │
        │ eip    = trampolín │
        │ err    = 0    │
        │ int_no = 32   │
        │ eax..edi = 0  │  ← esp de la tarea
        └──────────────┘
```

La primera vez que el planificador devuelve ese `esp`, el epílogo del stub hace `popal`,
`add $8`, `iret` y la tarea empieza a ejecutar el trampolín, que llama a su función y,
si vuelve, marca la tarea como muerta.

En cada tick (100 Hz) el IRQ0:

1. guarda `esp` de la tarea actual;
2. elige la siguiente tarea viva en la lista circular;
3. devuelve su `esp` al stub, que reanuda esa pila.

`preempt_disable` lógico (`sched_lock`) evita cambios de tarea en medio de una escritura
a consola.

---

## 8. Llamadas al sistema

`int 0x80` es una puerta de interrupción con DPL 3 (preparada para futuro modo usuario).
El número de llamada va en `EAX` y los argumentos en `EBX`, `ECX`, `EDX`; el resultado se
escribe en `EAX` del marco guardado.

| EAX | Llamada | Argumentos |
|---|---|---|
| 1 | `write` | `EBX` cadena, `ECX` longitud |
| 2 | `exit` | `EBX` código |
| 3 | `getpid` | — |
| 4 | `uptime` | — |

---

## 9. RAMFS

16 nodos fijos, nombres de hasta 31 caracteres y datos de hasta 8 KiB reservados del heap
la primera vez que se escribe. Sin directorios, sin permisos, sin persistencia.

---

## 10. Escritorio gráfico

### 10.1 Del modo texto al framebuffer

`gfx_init(800, 600)` busca en el bus PCI el dispositivo `1234:1111` (la tarjeta SVGA de
QEMU/v86), programa el **dispositivo Bochs VBE** por los puertos `0x1CE` (índice) y `0x1CF`
(dato):

| Índice | Valor |
|---|---|
| 1 | ancho (800) |
| 2 | alto (600) |
| 3 | 8 bits por píxel |
| 4 | `0x41` = habilitar + framebuffer lineal |

Después lee el BAR0 de la configuración PCI, mapea con `paging_map_4mb` todas las páginas
de 4 MiB que cubren los 480 000 bytes del framebuffer y reserva con `kmalloc` un *back
buffer* del mismo tamaño. **Todo el dibujo se hace en el back buffer**; `gfx_flush(x, y, w,
h)` copia el rectángulo a la tarjeta y `gfx_flush_all()` repinta la pantalla completa.

Como en modo SVGA la ventana bancarizada se solapa con `0xB8000`, al entrar se desactiva la
consola de texto (`vga_set_enabled(false)`) para que sus escrituras no ensucien el
framebuffer. Si la tarjeta no expone el modo lineal, `gfx_init` falla, `gui_boot` devuelve
error y el kernel sigue con la consola de texto de 80×25 como respaldo.

La paleta es fija: 24 entradas de la DAC con los colores de la interfaz (fondo, paneles,
bordes, verde, cian, ámbar, rojo, blanco…). La fuente son los 2 KiB de glifos 8×8 que la
etapa 2 copia del BIOS a `0x00006000` antes de entrar en modo protegido.

### 10.2 Consola abstracta

`include/console.h` define una interfaz mínima:

```c
typedef struct console {
    int cols, rows;
    void *data;
    void (*putc)(struct console *c, char ch);
    void (*clear)(struct console *c);
    void (*set_color)(struct console *c, uint8_t fg, uint8_t bg);
    void (*status)(struct console *c, const char *text);
} console_t;
```

Hay dos implementaciones: la de texto VGA (`vga_console()`) y la de la ventana del
escritorio (`terminal_console()`), que guarda una rejilla de 76×42 celdas de 8×10 píxeles
con color por celda. Cada carácter que llega marca la ventana del terminal como dañada; el
repintado lo hace el gestor de ventanas con el resto de la escena, nunca desde la consola.
`kputc`, `kprintf` y la barra de estado hablan con `console_use()` y no saben en qué modo
están, así que la shell, las tareas y las autopruebas funcionan igual dentro de una ventana.

### 10.3 Teclado y ratón

- El teclado PS/2 sigue siendo el mismo; además de las teclas normales, el IRQ1 guarda en un
  anillo aparte `F1`, `F2`, `F3`, `ESC` y las flechas.
- `input_set_source(fn)` permite que `input_getchar()` lea de la cola del terminal gráfico en
  vez del teclado y del puerto serie. Así el bucle de la shell no cambia.
- El ratón se inicializa con las interrupciones enmascaradas: se manda `0xA8` (activar puerto
  auxiliar), se lee la configuración con `0x20`, se reescribe con `0x60` poniendo el bit de
  IRQ12, y luego `0xF6` + `0xF4` al propio ratón. Los *ACK* (`0xFA`) llegan por el mismo
  puerto, por eso se leen por sondeo antes de desenmascarar IRQ12 (y la cascada IRQ2).
- El controlador 8042 solo guarda un byte a la vez: si llegan varios paquetes seguidos, el
  segundo se queda esperando y su IRQ se pierde. Por eso `mouse_drain()` no lee un byte,
  sino que **vacía el búfer**: mientras el bit 5 del puerto `0x64` indique datos auxiliares
  lee el puerto `0x60`, descarta ACK y reengancha el flujo en el byte que lleva el bit 3
  (`0x08`) del primer byte del paquete. `mouse_irq()` y `mouse_poll()` (llamado por el
  escritorio en cada vuelta) ejecutan el mismo drenaje.
- Los botones se consumen como **eventos de flanco**: `mouse_take_click()` devuelve `true`
  una sola vez por pulsación. Así un clic no se pierde si el botón ya está suelto cuando el
  escritorio lo consulta, algo habitual en los emuladores que solo emiten la transición.

### 10.4 Gestor de ventanas

`kernel/gui/wm.c` es un compositor pequeño pero completo:

- **Pila de ventanas** ordenada (la última es la de arriba, la que tiene el foco) y un
  *pool* fijo de 8 ventanas con su función de dibujo, de clic y de tecla.
- **Daño por rectángulos**: cada operación apunta el área que cambia. Antes de pintar,
  `grow_damage()` la agranda hasta cubrir cualquier ventana que la solape, de modo que
  repintar de atrás hacia delante siempre respeta la superposición sin listas de oclusión.
- **Pintado**: fondo, iconos del escritorio, ventanas de atrás hacia delante (sombra, marco,
  barra de título con sus tres botones, contenido recortado), barra de tareas y menú. El
  resultado se vuelca con `gfx_flush`.
- **Interacción**: arrastre por la barra de título, redimensionado por la esquina inferior
  derecha (mínimo por ventana), minimizar a la barra de tareas, maximizar guardando la
  geometría anterior, cerrar y doble clic (que maximiza o restaura).
- **Barra de tareas**: botón `Inicio` con menú de seis aplicaciones, un botón por ventana con
  el foco marcado en verde y el reloj del CMOS refrescado cada 50 ticks.
- **Cursor**: se dibuja sobre el *back buffer* ya volcado; antes de pintar se restaura el
  rectángulo guardado bajo el puntero y al terminar se vuelve a dibujar.

Las aplicaciones (`kernel/gui/apps.c`) son ventanas normales: Archivos lista el RAMFS y
abre el editor con doble clic, el Editor carga y guarda texto (botón `Guardar`, `Ctrl+S` y
autoguardado al cerrar), la Calculadora acumula operaciones, y Sistema y Acerca de se
repintan con datos vivos del kernel.

---

## 11. Depuración

- Todo lo que se imprime por consola se duplica en **COM1** (115200 8N1), de ahí que
  `make test` pueda leer la sesión completa.
- La shell acepta entrada por teclado PS/2 **o** por el puerto serie, lo que permite
  automatizar comandos desde Node.
- `test` cubre asignador de marcos, heap, strings, RAMFS, temporizador y planificador.
- `make test` incluye además comprobaciones del escritorio: modo 800×600×8, arranque con la
  ventana del terminal, menú de inicio, arrastre, minimizar y restaurar desde la barra de
  tareas, doble clic que abre el editor y autoguardado en el RAMFS.
- La shell tiene dos ayudas para automatizar: `win` lista las ventanas (id, título, geometría,
  estado y foco) y `mouseto X Y` coloca el puntero sin depender de deltas.
- `make shots` y `make shots-gui` conducen el escritorio con `tools/gui-shot.mjs` (guion de
  clics y teclas), vuelcan el framebuffer SVGA y la paleta, y generan las imágenes de
  `docs/` con `tools/render-framebuffer.py`.
