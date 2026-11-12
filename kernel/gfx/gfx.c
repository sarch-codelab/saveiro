#include <gfx.h>
#include <io.h>
#include <heap.h>
#include <paging.h>
#include <string.h>
#include <console.h>
#include <vga.h>

#define DISPI_INDEX 0x1CE
#define DISPI_DATA  0x1CF

static gfx_info_t info;
static bool ready;
static uint32_t pci_slot = 0xFFFF;

/* ------------------------------------------------------------------ */
/* PCI: localizar la tarjeta VGA compatible                            */
/* ------------------------------------------------------------------ */

static uint32_t pci_read32(uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) |
                       ((uint32_t)offset & 0xFCu);

    outl(0xCF8, address);
    return inl(0xCFC);
}

static void pci_write32(uint8_t slot, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) |
                       ((uint32_t)offset & 0xFCu);

    outl(0xCF8, address);
    outl(0xCFC, value);
}

static bool pci_find_vga(void)
{
    uint8_t slot;

    for (slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read32(slot, 0, 0);
        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        uint16_t device = (uint16_t)(id >> 16);

        /* 1234:1111 es la VGA estándar de QEMU y de v86 */
        if (vendor == 0x1234 && device == 0x1111) {
            uint32_t command = pci_read32(slot, 0, 0x04);

            pci_write32(slot, 0, 0x04, command | 0x0003);  /* I/O + memoria */
            pci_slot = slot;
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Dispositivo VBE (interfaz Bochs/QEMU, usada por v86)                */
/* ------------------------------------------------------------------ */

static void dispi_write(uint16_t index, uint16_t value)
{
    outw(DISPI_INDEX, index);
    outw(DISPI_DATA, value);
}

static uint16_t dispi_read(uint16_t index)
{
    outw(DISPI_INDEX, index);
    return inw(DISPI_DATA);
}

static bool dispi_set_mode(uint32_t width, uint32_t height)
{
    if (dispi_read(0) < 0xB0C0)
        return false;

    dispi_write(1, (uint16_t)width);
    dispi_write(2, (uint16_t)height);
    dispi_write(3, 8);              /* 8 bits por píxel */
    dispi_write(4, 0x41);           /* habilitar + framebuffer lineal */

    if (dispi_read(1) != width || dispi_read(2) != height)
        return false;

    return true;
}

/* ------------------------------------------------------------------ */
/* Paleta                                                              */
/* ------------------------------------------------------------------ */

static const uint8_t palette[24][3] = {
    { 0x07, 0x0a, 0x0d },   /* 0  fondo */
    { 0x0b, 0x13, 0x18 },   /* 1  escritorio */
    { 0x0e, 0x1a, 0x21 },   /* 2  panel */
    { 0x12, 0x22, 0x2b },   /* 3  panel alt */
    { 0x1c, 0x33, 0x40 },   /* 4  borde */
    { 0x2c, 0x4d, 0x5e },   /* 5  borde claro */
    { 0xdc, 0xeb, 0xe2 },   /* 6  texto */
    { 0x86, 0x9e, 0x93 },   /* 7  texto tenue */
    { 0x46, 0xf0, 0x8a },   /* 8  verde */
    { 0x7c, 0xe8, 0xff },   /* 9  cian */
    { 0xf5, 0xc9, 0x6b },   /* 10 ámbar */
    { 0xff, 0x6b, 0x7a },   /* 11 rojo */
    { 0xff, 0xff, 0xff },   /* 12 blanco */
    { 0x06, 0x11, 0x0d },   /* 13 fondo del terminal */
    { 0xc9, 0xe8, 0xd6 },   /* 14 texto del terminal */
    { 0x7d, 0xef, 0xa8 },   /* 15 verde del terminal */
    { 0x4f, 0x6b, 0x5c },   /* 16 dimensión */
    { 0x13, 0x3d, 0x2c },   /* 17 barra activa */
    { 0x17, 0x28, 0x32 },   /* 18 barra inactiva */
    { 0x03, 0x06, 0x08 },   /* 19 sombra */
    { 0x4e, 0x8f, 0xd6 },   /* 20 azul */
    { 0x0a, 0x24, 0x1a },   /* 21 verde oscuro */
    { 0xb9, 0x95, 0x4d },   /* 22 amarillo tenue */
    { 0x3a, 0x4a, 0x55 }    /* 23 gris */
};

void gfx_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    outb(0x3C8, index);
    outb(0x3C9, (uint8_t)(r >> 2));
    outb(0x3C9, (uint8_t)(g >> 2));
    outb(0x3C9, (uint8_t)(b >> 2));
}

void gfx_apply_palette(void)
{
    uint8_t i;

    for (i = 0; i < 24; i++)
        gfx_set_palette(i, palette[i][0], palette[i][1], palette[i][2]);

    for (i = 24; i < 32; i++)
        gfx_set_palette(i, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Inicialización                                                      */
/* ------------------------------------------------------------------ */

int gfx_init(uint32_t width, uint32_t height)
{
    uint32_t phys;
    uint32_t size;
    uint32_t page;

    if (ready)
        return 0;

    if (!pci_find_vga())
        return -1;

    if (!dispi_set_mode(width, height))
        return -1;

    phys = pci_read32((uint8_t)pci_slot, 0, 0x10) & 0xFFFFFFF0u;
    if (!phys)
        return -1;

    size = width * height;

    for (page = phys & 0xFFC00000u; page < phys + size; page += 0x400000u)
        paging_map_4mb(page);

    info.width = (int)width;
    info.height = (int)height;
    info.pitch = (int)width;
    info.bpp = 8;
    info.phys = phys;
    info.fb = (uint8_t *)phys;
    info.back = (uint8_t *)kmalloc(width * height);
    if (!info.back)
        return -1;

    /* En modo SVGA las escrituras a 0xB8000 caen dentro del framebuffer
     * (ventana bancarizada), así que apagamos la consola de texto. */
    vga_set_enabled(false);

    gfx_apply_palette();
    gfx_clear(GFX_DESKTOP);
    gfx_flush_all();

    ready = true;
    return 0;
}

gfx_info_t *gfx_get_info(void)
{
    return ready ? &info : NULL;
}

void gfx_clear(uint8_t color)
{
    if (!ready)
        return;
    memset(info.back, color, (size_t)info.width * (size_t)info.height);
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */

void gfx_pixel(int x, int y, uint8_t color)
{
    if (!ready || x < 0 || y < 0 || x >= info.width || y >= info.height)
        return;

    info.back[y * info.pitch + x] = color;
}

uint8_t gfx_get_pixel(int x, int y)
{
    if (!ready || x < 0 || y < 0 || x >= info.width || y >= info.height)
        return 0;

    return info.back[y * info.pitch + x];
}

void gfx_fill(int x, int y, int w, int h, uint8_t color)
{
    int row;

    if (!ready || w <= 0 || h <= 0)
        return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > info.width) w = info.width - x;
    if (y + h > info.height) h = info.height - y;
    if (w <= 0 || h <= 0)
        return;

    for (row = 0; row < h; row++)
        memset(info.back + (y + row) * info.pitch + x, color, (size_t)w);
}

void gfx_hline(int x, int y, int w, uint8_t color)
{
    gfx_fill(x, y, w, 1, color);
}

void gfx_vline(int x, int y, int h, uint8_t color)
{
    gfx_fill(x, y, 1, h, color);
}

void gfx_frame(int x, int y, int w, int h, uint8_t color)
{
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_char(int x, int y, char ch, uint8_t fg, uint8_t bg)
{
    const uint8_t *glyph = (const uint8_t *)(GFX_FONT_ADDR + ((uint8_t)ch * GFX_FONT_H));
    int row;
    int col;

    for (row = 0; row < GFX_FONT_H; row++) {
        uint8_t bits = glyph[row];

        for (col = 0; col < GFX_FONT_W; col++) {
            bool on = (bits & (0x80u >> col)) != 0;

            gfx_pixel(x + col, y + row, on ? fg : bg);
        }
    }
}

void gfx_char_alpha(int x, int y, char ch, uint8_t fg)
{
    const uint8_t *glyph = (const uint8_t *)(GFX_FONT_ADDR + ((uint8_t)ch * GFX_FONT_H));
    int row;
    int col;

    for (row = 0; row < GFX_FONT_H; row++) {
        uint8_t bits = glyph[row];

        for (col = 0; col < GFX_FONT_W; col++) {
            if (bits & (0x80u >> col))
                gfx_pixel(x + col, y + row, fg);
        }
    }
}

int gfx_text(int x, int y, const char *text, uint8_t fg, uint8_t bg)
{
    int start = x;

    while (*text) {
        if (*text == '\n') {
            x = start;
            y += GFX_FONT_H;
        } else {
            gfx_char(x, y, *text, fg, bg);
            x += GFX_FONT_W;
        }
        text++;
    }

    return x;
}

int gfx_text_alpha(int x, int y, const char *text, uint8_t fg)
{
    int start = x;

    while (*text) {
        if (*text == '\n') {
            x = start;
            y += GFX_FONT_H;
        } else {
            gfx_char_alpha(x, y, *text, fg);
            x += GFX_FONT_W;
        }
        text++;
    }

    return x;
}

int gfx_text_width(const char *text)
{
    return (int)strlen(text) * GFX_FONT_W;
}

/* ------------------------------------------------------------------ */
/* Volcado a la memoria de vídeo                                       */
/* ------------------------------------------------------------------ */

void gfx_flush(int x, int y, int w, int h)
{
    int row;

    if (!ready)
        return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > info.width) w = info.width - x;
    if (y + h > info.height) h = info.height - y;
    if (w <= 0 || h <= 0)
        return;

    for (row = 0; row < h; row++) {
        memcpy(info.fb + (y + row) * info.pitch + x,
               info.back + (y + row) * info.pitch + x,
               (size_t)w);
    }
}

void gfx_flush_all(void)
{
    gfx_flush(0, 0, info.width, info.height);
}

/* ------------------------------------------------------------------ */
/* Cursor del ratón                                                    */
/* ------------------------------------------------------------------ */

static const uint8_t cursor_shape[12] = {
    0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC,
    0xFE, 0xFF, 0xF8, 0xD8, 0x8C, 0x06
};

void gfx_cursor_draw(int x, int y)
{
    int row;
    int col;

    if (!ready)
        return;

    for (row = 0; row < 12; row++) {
        for (col = 0; col < 8; col++) {
            if (!(cursor_shape[row] & (0x80u >> col)))
                continue;

            /* sombra */
            if (x + col + 1 >= 0 && x + col + 1 < info.width &&
                y + row + 1 >= 0 && y + row + 1 < info.height)
                info.fb[(y + row + 1) * info.pitch + x + col + 1] = GFX_SHADOW;

            if (x + col >= 0 && x + col < info.width &&
                y + row >= 0 && y + row < info.height)
                info.fb[(y + row) * info.pitch + x + col] = GFX_WHITE;
        }
    }
}

void gfx_restore(int x, int y, int w, int h)
{
    gfx_flush(x, y, w, h);
}

void gfx_shutdown(void)
{
    if (!ready)
        return;

    /* Volver al modo de texto estandar de VGA: deshabilitamos el
     * extended mode y reactivamos la consola de texto. */
    dispi_write(4, 0x0000);

    if (info.back) {
        kfree(info.back);
        info.back = NULL;
    }

    ready = false;
    vga_set_enabled(true);
    vga_clear();
}
