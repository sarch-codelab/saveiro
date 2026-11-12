#ifndef SAVEIRO_CONSOLE_H
#define SAVEIRO_CONSOLE_H

#include <types.h>

typedef struct console console_t;

struct console {
    int cols;
    int rows;
    void *data;
    void (*putc)(console_t *con, char c);
    void (*clear)(console_t *con);
    void (*set_color)(console_t *con, uint8_t fg, uint8_t bg);
    void (*status)(console_t *con, const char *text);
};

void console_init(console_t *con);
console_t *console_current(void);
void console_use(console_t *con);
void con_putc(char c);
void con_clear(void);
void con_set_color(uint8_t fg, uint8_t bg);
void con_reset_color(void);
void con_status(const char *text);

void kputc(char c);
void kputs(const char *s);
void kwrite(const char *s, size_t len);
int  kprintf(const char *fmt, ...);
int  ksnprintf(char *buf, size_t cap, const char *fmt, ...);

#endif
