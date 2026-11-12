#include <mouse.h>
#include <io.h>
#include <pic.h>

#define PS2_STATUS 0x64
#define PS2_DATA   0x60

#define STATUS_OUTPUT_FULL 0x01
#define STATUS_INPUT_FULL  0x02

static uint8_t packet[3];
static int   packet_index;
static int   accumulated_dx;
static int   accumulated_dy;
static uint8_t buttons;
static bool  updated;
static bool  detected;
static uint32_t packet_count;

static bool wait_input_empty(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & STATUS_INPUT_FULL) == 0)
            return true;
    }

    return false;
}

static bool wait_output_full(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & STATUS_OUTPUT_FULL)
            return true;
    }

    return false;
}

static void mouse_write(uint8_t value)
{
    if (!wait_input_empty())
        return;

    outb(PS2_STATUS, 0xD4);     /* siguiente byte va al ratón */
    if (!wait_input_empty())
        return;

    outb(PS2_DATA, value);

    if (wait_output_full())
        (void)inb(PS2_DATA);    /* ACK */
}

void mouse_init(void)
{
    uint8_t config;

    packet_index = 0;
    accumulated_dx = 0;
    accumulated_dy = 0;
    buttons = 0;
    updated = false;
    packet_count = 0;

    /* Durante la inicializacion el controlador devuelve ACK y el byte de
     * configuracion por el mismo puerto: con las interrupciones de teclado y
     * raton enmascaradas los leemos por sondeo y no se pierde nada. */
    pic_mask(1);
    pic_mask(2);
    pic_mask(12);
    cli();

    /* activar el puerto auxiliar */
    if (wait_input_empty())
        outb(PS2_STATUS, 0xA8);

    /* leer la configuracion del controlador */
    if (wait_input_empty())
        outb(PS2_STATUS, 0x20);

    if (wait_output_full()) {
        config = inb(PS2_DATA);
        config |= 0x02;             /* IRQ12 */
        config &= (uint8_t)~0x20;   /* reloj del raton activo */

        if (wait_input_empty())
            outb(PS2_STATUS, 0x60);
        if (wait_input_empty())
            outb(PS2_DATA, config);
    }

    mouse_write(0xF6);          /* valores por defecto */
    mouse_write(0xF4);          /* empezar a enviar paquetes */

    /* descartar cualquier byte pendiente antes de activar la interrupcion */
    {
        int i;

        for (i = 0; i < 16 && (inb(PS2_STATUS) & STATUS_OUTPUT_FULL); i++)
            (void)inb(PS2_DATA);
    }

    detected = true;

    sti();
    pic_unmask(1);
    pic_unmask(2);              /* cascada del PIC esclavo */
    pic_unmask(12);
}

void mouse_irq(void)
{
    uint8_t byte = inb(PS2_DATA);

    /* ACK / BAT: no forman parte de ningun paquete */
    if (byte == 0xFA || byte == 0xAA) {
        packet_index = 0;
        return;
    }

    /* el primer byte de un paquete siempre lleva el bit 3 a 1 */
    if (packet_index == 0 && (byte & 0x08) == 0)
        return;

    packet[packet_index++] = byte;

    if (packet_index < 3)
        return;

    packet_index = 0;

    {
        int dx = (int)(int8_t)packet[1];
        int dy = -(int)(int8_t)packet[2];

        accumulated_dx += dx;
        accumulated_dy += dy;
        buttons = (uint8_t)(packet[0] & 0x07);
        updated = true;
        packet_count++;
    }
}

bool mouse_read(mouse_event_t *event)
{
    if (!updated)
        return false;

    event->dx = accumulated_dx;
    event->dy = accumulated_dy;
    event->left = (buttons & 0x01) != 0;
    event->right = (buttons & 0x02) != 0;
    event->middle = (buttons & 0x04) != 0;

    accumulated_dx = 0;
    accumulated_dy = 0;
    updated = false;

    return true;
}

bool mouse_detected(void)
{
    return detected;
}

uint32_t mouse_packets(void)
{
    return packet_count;
}
