#ifndef SAVEIRO_GFX_H
#define SAVEIRO_GFX_H

#include <types.h>

/* Paleta del escritorio (índices 0..23 de la DAC) */
#define GFX_BG          0
#define GFX_DESKTOP     1
#define GFX_PANEL       2
#define GFX_PANEL_ALT   3
#define GFX_BORDER      4
#define GFX_BORDER_LT   5
#define GFX_TEXT        6
#define GFX_MUTED       7
#define GFX_GREEN       8
#define GFX_CYAN        9
#define GFX_AMBER       10
#define GFX_RED         11
#define GFX_WHITE       12
#define GFX_TERM_BG     13
#define GFX_TERM_TEXT   14
#define GFX_TERM_GREEN  15
#define GFX_TERM_DIM    16
#define GFX_TITLE_ON    17
#define GFX_TITLE_OFF   18
#define GFX_SHADOW      19
#define GFX_BLUE        20
#define GFX_DARK_GREEN  21
#define GFX_YELLOW_DIM  22
#define GFX_GRAY        23

#define GFX_FONT_ADDR   0x6000
#define GFX_FONT_W      8
#define GFX_FONT_H      8

typedef struct {
    int width;
    int height;
    int pitch;
    int bpp;
    uint32_t phys;
    uint8_t *fb;
    uint8_t *back;
} gfx_info_t;

int          gfx_init(uint32_t width, uint32_t height);
gfx_info_t  *gfx_get_info(void);

void gfx_clear(uint8_t color);
void gfx_pixel(int x, int y, uint8_t color);
uint8_t gfx_get_pixel(int x, int y);
void gfx_fill(int x, int y, int w, int h, uint8_t color);
void gfx_hline(int x, int y, int w, uint8_t color);
void gfx_vline(int x, int y, int h, uint8_t color);
void gfx_frame(int x, int y, int w, int h, uint8_t color);

void gfx_char(int x, int y, char ch, uint8_t fg, uint8_t bg);
void gfx_char_alpha(int x, int y, char ch, uint8_t fg);
int  gfx_text(int x, int y, const char *text, uint8_t fg, uint8_t bg);
int  gfx_text_alpha(int x, int y, const char *text, uint8_t fg);
int  gfx_text_width(const char *text);

void gfx_flush(int x, int y, int w, int h);
void gfx_flush_all(void);

void gfx_cursor_draw(int x, int y);
void gfx_restore(int x, int y, int w, int h);
void gfx_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void gfx_apply_palette(void);
void gfx_shutdown(void);

#endif
