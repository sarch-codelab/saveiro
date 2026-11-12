#ifndef SAVEIRO_KEYBOARD_H
#define SAVEIRO_KEYBOARD_H

#include <types.h>

void keyboard_init(void);
void keyboard_irq(void);
int  keyboard_poll(void);
int  input_getchar(void);

#endif
