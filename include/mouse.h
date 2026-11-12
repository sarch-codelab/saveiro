#ifndef SAVEIRO_MOUSE_H
#define SAVEIRO_MOUSE_H

#include <types.h>

typedef struct {
    int  dx;
    int  dy;
    bool left;
    bool right;
    bool middle;
} mouse_event_t;

void mouse_init(void);
void mouse_irq(void);

/* Consume los movimientos acumulados. Devuelve false si no hubo nada. */
bool mouse_read(mouse_event_t *event);
bool mouse_detected(void);
uint32_t mouse_packets(void);

#endif
