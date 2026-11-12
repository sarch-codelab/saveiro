#include <serial.h>
#include <io.h>

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* sin interrupciones */
    outb(COM1 + 3, 0x80);   /* DLAB */
    outb(COM1 + 0, 0x01);   /* divisor 1 -> 115200 baudios */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8N1 */
    outb(COM1 + 2, 0xC7);   /* FIFO */
    outb(COM1 + 4, 0x0B);   /* DTR, RTS, OUT2 */
    outb(COM1 + 1, 0x00);
}

void serial_putc(char c)
{
    int spin = 100000;

    while ((inb(COM1 + 5) & 0x20) == 0 && spin-- > 0)
        ;

    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    while (*s)
        serial_putc(*s++);
}

int serial_poll(void)
{
    if ((inb(COM1 + 5) & 0x01) == 0)
        return -1;

    return (int)inb(COM1);
}
