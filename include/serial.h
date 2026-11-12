#ifndef SAVEIRO_SERIAL_H
#define SAVEIRO_SERIAL_H

#include <types.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
int  serial_poll(void);   /* devuelve -1 si no hay dato */

#endif
