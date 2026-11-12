#include <console.h>
#include <vga.h>
#include <serial.h>

void kputc(char c)
{
    vga_putc(c);

    if (c == '\n')
        serial_putc('\r');
    serial_putc(c);
}

void kputs(const char *s)
{
    while (*s)
        kputc(*s++);
}

void kwrite(const char *s, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
        kputc(s[i]);
}
