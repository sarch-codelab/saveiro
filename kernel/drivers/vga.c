#include <vga.h>
#include <io.h>
#include <string.h>
#include <console.h>

#define VGA_MEM ((volatile uint16_t *)0xB8000)

static int cursor_row;
static int cursor_col;
static uint8_t color = 0x07;
static bool enabled = true;

static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);

    if (!enabled)
        return;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void)
{
    int r;
    int c;

    for (r = 1; r < VGA_CONSOLE_ROWS; r++) {
        for (c = 0; c < VGA_COLS; c++)
            VGA_MEM[(r - 1) * VGA_COLS + c] = VGA_MEM[r * VGA_COLS + c];
    }

    for (c = 0; c < VGA_COLS; c++)
        VGA_MEM[(VGA_CONSOLE_ROWS - 1) * VGA_COLS + c] =
            (uint16_t)(' ' | ((uint16_t)color << 8));

    if (cursor_row > 0)
        cursor_row--;
}

void vga_init(void)
{
    cursor_row = 0;
    cursor_col = 0;
    color = 0x07;
    vga_clear();
}

void vga_clear(void)
{
    int i;

    if (!enabled)
        return;

    for (i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEM[i] = (uint16_t)(' ' | 0x0700);

    cursor_row = 0;
    cursor_col = 0;
    update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
    color = (uint8_t)((fg & 0x0F) | ((bg & 0x0F) << 4));
}

void vga_reset_color(void)
{
    color = 0x07;
}

void vga_move_cursor(int row, int col)
{
    cursor_row = row;
    cursor_col = col;
    update_cursor();
}

void vga_putc(char c)
{
    if (!enabled)
        return;

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        do {
            vga_putc(' ');
        } while ((cursor_col & 7) != 0);
        return;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        VGA_MEM[cursor_row * VGA_COLS + cursor_col] = (uint16_t)(' ' | ((uint16_t)color << 8));
        update_cursor();
        return;
    } else {
        VGA_MEM[cursor_row * VGA_COLS + cursor_col] =
            (uint16_t)((uint8_t)c | ((uint16_t)color << 8));
        cursor_col++;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    while (cursor_row >= VGA_CONSOLE_ROWS)
        scroll();

    update_cursor();
}

void vga_write(const char *s)
{
    while (*s)
        vga_putc(*s++);
}

void vga_set_enabled(bool on)
{
    enabled = on;
}

void vga_status_bar(const char *text)
{
    int i;
    int len = 0;

    if (!enabled)
        return;

    while (len < VGA_COLS && text[len])
        len++;

    for (i = 0; i < VGA_COLS; i++) {
        char c = (i < len) ? text[i] : ' ';

        VGA_MEM[VGA_CONSOLE_ROWS * VGA_COLS + i] =
            (uint16_t)((uint8_t)c | 0x7000);
    }
}

/* ---- adaptador de consola sobre la memoria de texto VGA ---- */

static void vga_con_putc(console_t *con, char c)
{
    UNUSED(con);
    vga_putc(c);
}

static void vga_con_clear(console_t *con)
{
    UNUSED(con);
    vga_clear();
}

static void vga_con_set_color(console_t *con, uint8_t fg, uint8_t bg)
{
    UNUSED(con);
    vga_set_color(fg, bg);
}

static void vga_con_status(console_t *con, const char *text)
{
    UNUSED(con);
    vga_status_bar(text);
}

console_t *vga_console(void)
{
    static console_t con = {
        VGA_COLS, VGA_CONSOLE_ROWS, NULL,
        vga_con_putc, vga_con_clear, vga_con_set_color, vga_con_status
    };

    return &con;
}
