/* Escritorio de SaveiroOS: arranque, consola del terminal y aplicacion
 * de terminal. El gestor de ventanas vive en wm.c y las demas aplicaciones
 * en apps.c.
 */

#include <types.h>
#include <console.h>
#include <gfx.h>
#include <keyboard.h>
#include <mouse.h>
#include <pit.h>
#include <rtc.h>
#include <string.h>
#include <vga.h>
#include <serial.h>
#include <task.h>
#include <io.h>

#include "gui_internal.h"

#define TERM_COLS 76
#define TERM_ROWS 42
#define TERM_CELL_H 10

typedef struct {
    uint8_t ch;
    uint8_t fg;
    uint8_t bg;
} term_cell_t;

static console_t term_console;
static term_cell_t cells[TERM_ROWS][TERM_COLS];
static int term_cx;
static int term_cy;
static uint8_t term_fg = GFX_TERM_TEXT;
static uint8_t term_bg = GFX_TERM_BG;

static bool gui_running;
static wm_window_t *terminal_window;

/* entrada de teclado hacia la shell */
#define INPUT_SIZE 256
static volatile char input_buffer[INPUT_SIZE];
static volatile int input_head;
static volatile int input_tail;

/* ------------------------------------------------------------------ */
/* Paleta de la consola                                                */
/* ------------------------------------------------------------------ */

static uint8_t palette_of(uint8_t vga_color)
{
    static const uint8_t map[16] = {
        GFX_TERM_BG, GFX_BLUE, GFX_GREEN, GFX_CYAN,
        GFX_RED, GFX_CYAN, GFX_AMBER, GFX_TEXT,
        GFX_MUTED, GFX_CYAN, GFX_TERM_GREEN, GFX_CYAN,
        GFX_RED, GFX_CYAN, GFX_AMBER, GFX_WHITE
    };

    return map[vga_color & 0x0F];
}

/* ------------------------------------------------------------------ */
/* Consola del terminal                                                */
/* ------------------------------------------------------------------ */

static void clear_row(int row, uint8_t bg)
{
    int col;

    for (col = 0; col < TERM_COLS; col++) {
        cells[row][col].ch = ' ';
        cells[row][col].fg = term_fg;
        cells[row][col].bg = bg;
    }
}

static void term_scroll(void)
{
    int row;

    for (row = 1; row < TERM_ROWS; row++)
        memcpy(cells[row - 1], cells[row], sizeof(cells[0]));

    clear_row(TERM_ROWS - 1, term_bg);
    term_cy = TERM_ROWS - 1;
}

static void term_putc(console_t *con, char c)
{
    UNUSED(con);

    if (c == '\n') {
        term_cx = 0;
        term_cy++;
    } else if (c == '\r') {
        term_cx = 0;
    } else if (c == '\b') {
        if (term_cx > 0) {
            term_cx--;
        } else if (term_cy > 0) {
            term_cy--;
            term_cx = TERM_COLS - 1;
        }
        cells[term_cy][term_cx].ch = ' ';
        wm_invalidate();
        return;
    } else if (c == '\t') {
        do {
            term_putc(con, ' ');
        } while (term_cx % 8 != 0);
        return;
    } else {
        cells[term_cy][term_cx].ch = (uint8_t)c;
        cells[term_cy][term_cx].fg = term_fg;
        cells[term_cy][term_cx].bg = term_bg;
        term_cx++;

        if (term_cx >= TERM_COLS) {
            term_cx = 0;
            term_cy++;
        }
    }

    while (term_cy >= TERM_ROWS)
        term_scroll();

    /* solo se repinta la ventana del terminal */
    wm_window_damage(terminal_window);
}

static void term_clear(console_t *con)
{
    int row;

    UNUSED(con);

    for (row = 0; row < TERM_ROWS; row++)
        clear_row(row, term_bg);

    term_cx = 0;
    term_cy = 0;
    wm_window_damage(terminal_window);
}

static void term_set_color(console_t *con, uint8_t fg, uint8_t bg)
{
    UNUSED(con);

    term_fg = palette_of(fg);
    term_bg = (bg == 0) ? GFX_TERM_BG : palette_of(bg);
}

static void term_status(console_t *con, const char *text)
{
    UNUSED(con);
    UNUSED(text);
}

static console_t *terminal_console(void)
{
    term_console.cols = TERM_COLS;
    term_console.rows = TERM_ROWS;
    term_console.data = NULL;
    term_console.putc = term_putc;
    term_console.clear = term_clear;
    term_console.set_color = term_set_color;
    term_console.status = term_status;

    return &term_console;
}

void terminal_reset(void)
{
    int row;

    for (row = 0; row < TERM_ROWS; row++)
        clear_row(row, GFX_TERM_BG);

    term_cx = 0;
    term_cy = 0;
    term_fg = GFX_TERM_TEXT;
    term_bg = GFX_TERM_BG;
    input_head = 0;
    input_tail = 0;
}

/* ------------------------------------------------------------------ */
/* Entrada                                                             */
/* ------------------------------------------------------------------ */

void terminal_push(char c)
{
    int next = (input_head + 1) % INPUT_SIZE;

    cli();
    if (next != input_tail) {
        input_buffer[input_head] = c;
        input_head = next;
    }
    sti();
}

void terminal_key(int key)
{
    terminal_push((char)key);
}

static int terminal_tryget(void)
{
    char c;
    int key;

    if (input_tail == input_head) {
        key = serial_poll();
        if (key >= 0)
            return key == '\r' ? '\n' : key;

        if (!gui_running) {
            key = keyboard_poll();
            if (key >= 0)
                return key == '\r' ? '\n' : key;
            __asm__ volatile("sti; hlt");
        }

        return -1;
    }

    cli();
    c = input_buffer[input_tail];
    input_tail = (input_tail + 1) % INPUT_SIZE;
    sti();

    return (uint8_t)c;
}

void terminal_attach(void)
{
    console_use(terminal_console());
    input_set_source(terminal_tryget);
}

/* ------------------------------------------------------------------ */
/* Dibujo del terminal                                                 */
/* ------------------------------------------------------------------ */

void terminal_draw(wm_window_t *win)
{
    int x0 = wm_content_x(win);
    int y0 = wm_content_y(win);
    int cx;
    int cy;
    int cw;
    int ch;
    int row0;
    int row1;
    int col0;
    int col1;
    int row;
    int col;

    gfx_fill(x0, y0, wm_content_w(win), wm_content_h(win), GFX_TERM_BG);

    wm_clip_rect(&cx, &cy, &cw, &ch);

    row0 = (cy - y0) / TERM_CELL_H;
    row1 = (cy + ch - y0) / TERM_CELL_H;
    col0 = (cx - x0) / 8;
    col1 = (cx + cw - x0) / 8;

    if (row0 < 0) row0 = 0;
    if (col0 < 0) col0 = 0;
    if (row1 > TERM_ROWS - 1) row1 = TERM_ROWS - 1;
    if (col1 > TERM_COLS - 1) col1 = TERM_COLS - 1;

    for (row = row0; row <= row1; row++) {
        for (col = col0; col <= col1; col++) {
            const term_cell_t *cell = &cells[row][col];

            gfx_char(x0 + col * 8, y0 + row * TERM_CELL_H, (char)cell->ch, cell->fg, cell->bg);
        }
    }

    /* cursor de texto */
    if (win == wm_focused() && term_cy < TERM_ROWS && term_cx < TERM_COLS) {
        int x = x0 + term_cx * 8;
        int y = y0 + term_cy * TERM_CELL_H + 7;

        if (x >= cx && x + 6 <= cx + cw && y >= cy && y + 2 <= cy + ch)
            gfx_fill(x, y, 6, 2, GFX_GREEN);
    }
}

/* ------------------------------------------------------------------ */
/* Aplicacion Terminal                                                 */
/* ------------------------------------------------------------------ */

static void terminal_click(wm_window_t *win, int x, int y, bool double_click)
{
    UNUSED(win);
    UNUSED(x);
    UNUSED(y);
    UNUSED(double_click);
}

static void terminal_window_key(wm_window_t *win, int key)
{
    UNUSED(win);
    terminal_key(key);
}

void app_terminal_open(void)
{
    if (terminal_window) {
        terminal_window->visible = true;
        wm_focus(terminal_window);
        return;
    }

    terminal_window = wm_create("Terminal", 70, 40, 660, 430);
    if (!terminal_window)
        return;

    terminal_window->draw = terminal_draw;
    terminal_window->click = terminal_click;
    terminal_window->key = terminal_window_key;

    wm_focus(terminal_window);
}

/* ------------------------------------------------------------------ */
/* Arranque                                                            */
/* ------------------------------------------------------------------ */

int gui_boot(void)
{
    if (gui_running)
        return 0;

    if (gfx_init(WM_SCREEN_W, WM_SCREEN_H) < 0)
        return -1;

    terminal_reset();
    terminal_attach();
    con_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    mouse_init();
    wm_start();

    gui_running = true;
    app_terminal_open();

    wm_invalidate();

    return 0;
}

int gui_is_active(void)
{
    return gui_running;
}

void gui_shutdown(void)
{
    if (!gui_running)
        return;

    gui_running = false;
    gfx_shutdown();
    console_use(vga_console());
}
