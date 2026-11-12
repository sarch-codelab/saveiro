#ifndef SAVEIRO_VGA_H
#define SAVEIRO_VGA_H

#include <types.h>

#define VGA_COLS 80
#define VGA_ROWS 25
/* Las filas 0..23 son la consola; la 24 es la barra de estado. */
#define VGA_CONSOLE_ROWS 24

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LIGHT_GREY,
    VGA_DARK_GREY,
    VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA,
    VGA_YELLOW,
    VGA_WHITE
};

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_write(const char *s);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_reset_color(void);
void vga_status_bar(const char *text);
void vga_move_cursor(int row, int col);

#endif
