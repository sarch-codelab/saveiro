#ifndef SAVEIRO_KEYBOARD_H
#define SAVEIRO_KEYBOARD_H

#include <types.h>

void keyboard_init(void);
void keyboard_irq(void);
int  keyboard_poll(void);
int  keyboard_poll_special(void);   /* F1..F3, ESC como 0x100+.. */
void input_set_source(int (*source)(void));
int  input_getchar(void);

#define KEY_F1 0x101
#define KEY_F2 0x102
#define KEY_F3 0x103

#endif
