#include <console.h>
#include <vga.h>
#include <serial.h>

static console_t *current;

void console_init(console_t *con)
{
    current = con;
}

console_t *console_current(void)
{
    return current;
}

void console_use(console_t *con)
{
    if (con)
        current = con;
}

void kputc(char c)
{
    if (current && current->putc)
        current->putc(current, c);

    if (c == '\n')
        serial_putc('\r');
    serial_putc(c);
}

void con_putc(char c)
{
    kputc(c);
}

void con_clear(void)
{
    if (current && current->clear)
        current->clear(current);
}

void con_set_color(uint8_t fg, uint8_t bg)
{
    if (current && current->set_color)
        current->set_color(current, fg, bg);
}

void con_reset_color(void)
{
    con_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

void con_status(const char *text)
{
    if (current && current->status)
        current->status(current, text);
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
