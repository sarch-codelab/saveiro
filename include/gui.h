#ifndef SAVEIRO_GUI_H
#define SAVEIRO_GUI_H

/* Arranca el escritorio. Devuelve 0 si hay framebuffer disponible. */
int  gui_boot(void);

/* 1 mientras el escritorio esta en marcha. */
int  gui_is_active(void);

/* Vuelve al modo texto (para depuracion). */
void gui_shutdown(void);

/* Lista las ventanas abiertas por la consola (depuracion). */
void gui_list_windows(void);

/* Coloca el puntero en una coordenada (utilidad de depuracion). */
void gui_mouse_warp(int x, int y);

#endif
