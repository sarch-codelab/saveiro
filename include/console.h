#ifndef SAVEIRO_CONSOLE_H
#define SAVEIRO_CONSOLE_H

#include <types.h>

void kputc(char c);
void kputs(const char *s);
void kwrite(const char *s, size_t len);
int  kprintf(const char *fmt, ...);
int  ksnprintf(char *buf, size_t cap, const char *fmt, ...);

#endif
