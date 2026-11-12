#ifndef SAVEIRO_PIC_H
#define SAVEIRO_PIC_H

#include <types.h>

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

void pic_init(void);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_eoi(uint8_t irq);

#endif
