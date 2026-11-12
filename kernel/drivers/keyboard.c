#include <keyboard.h>
#include <io.h>
#include <serial.h>

#define KBD_BUF_SIZE 128

static const char keymap[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char keymap_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static volatile uint8_t buffer[KBD_BUF_SIZE];
static volatile int head;
static volatile int tail;

static volatile uint8_t special[16];
static volatile int special_head;
static volatile int special_tail;

static int (*input_source)(void);
static bool shift;
static bool caps;
static bool extended;

static void push(char c)
{
    int next = (head + 1) % KBD_BUF_SIZE;

    if (next == tail)
        return;             /* buffer lleno: se descarta */

    buffer[head] = (uint8_t)c;
    head = next;
}

void keyboard_init(void)
{
    /* vaciar el búfer del controlador */
    while (inb(0x64) & 1)
        (void)inb(0x60);

    head = 0;
    tail = 0;
    special_head = 0;
    special_tail = 0;
    shift = false;
    caps = false;
    extended = false;
}

void keyboard_irq(void)
{
    uint8_t scancode = inb(0x60);
    char c;

    if (scancode == 0xE0) {
        extended = true;
        return;
    }

    if (extended) {
        extended = false;
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift = false;
        return;
    }
    if (scancode == 0x3A) {
        caps = !caps;
        return;
    }

    if (scancode == 0x3B || scancode == 0x3C || scancode == 0x3D) {
        int next = (special_head + 1) % 16;

        if (next != special_tail) {
            special[special_head] = (uint8_t)(scancode - 0x3B + 1);
            special_head = next;
        }
        return;
    }

    if (scancode == 0x01) {
        int next = (special_head + 1) % 16;

        if (next != special_tail) {
            special[special_head] = 27;
            special_head = next;
        }
        return;
    }
    if (scancode & 0x80)
        return;

    c = shift ? keymap_shift[scancode] : keymap[scancode];

    if (caps && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    else if (caps && shift && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    if (c)
        push(c);
}

int keyboard_poll(void)
{
    char c;

    if (tail == head)
        return -1;

    c = (char)buffer[tail];
    tail = (tail + 1) % KBD_BUF_SIZE;

    return (uint8_t)c;
}

int keyboard_poll_special(void)
{
    int value;

    if (special_tail == special_head)
        return -1;

    value = special[special_tail];
    special_tail = (special_tail + 1) % 16;

    return value;
}

void input_set_source(int (*source)(void))
{
    input_source = source;
}

int input_getchar(void)
{
    if (input_source) {
        int c;

        while ((c = input_source()) < 0)
            __asm__ volatile("sti; hlt");

        return c;
    }

    for (;;) {
        int c = keyboard_poll();

        if (c >= 0)
            return c == '\r' ? '\n' : c;

        c = serial_poll();
        if (c >= 0)
            return c == '\r' ? '\n' : c;

        __asm__ volatile("sti; hlt");
    }
}
