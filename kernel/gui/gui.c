#include <gui.h>
#include <gfx.h>
#include <console.h>
#include <keyboard.h>
#include <mouse.h>
#include <task.h>
#include <pit.h>
#include <rtc.h>
#include <pmm.h>
#include <heap.h>
#include <ramfs.h>
#include <string.h>
#include <io.h>
#include <pic.h>
#include <serial.h>
#include <vga.h>

/* ------------------------------------------------------------------ */
/* Geometría                                                           */
/* ------------------------------------------------------------------ */

#define SCREEN_W 800
#define SCREEN_H 600

#define TOPBAR_H   28
#define BOTTOM_Y   572
#define BOTTOM_H   28

#define WIN_COUNT 3
#define WIN_TERM  0
#define WIN_SYSTEM 1
#define WIN_FILES 2

#define TERM_X 12
#define TERM_Y 40
#define TERM_W 588
#define TERM_H 496
#define TERM_CX0 (TERM_X + 6)
#define TERM_CY0 (TERM_Y + 24)

#define TERM_COLS 72
#define TERM_ROWS 56
#define TERM_FOOTER_Y (TERM_Y + TERM_H - 12)

#define SYS_X 608
#define SYS_Y 40
#define SYS_W 180
#define SYS_H 238

#define FILES_X 608
#define FILES_Y 290
#define FILES_W 180
#define FILES_H 246
#define FILES_ROW0 (FILES_Y + 46)
#define FILES_ROW_H 14

#define VGA_COLS 80

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char *title;
} gui_window_t;

static const gui_window_t windows[WIN_COUNT] = {
    { TERM_X,  TERM_Y,  TERM_W,  TERM_H,  "Terminal" },
    { SYS_X,   SYS_Y,   SYS_W,   SYS_H,   "Sistema"  },
    { FILES_X, FILES_Y, FILES_W, FILES_H, "Archivos" },
};

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
static bool term_dirty;
static int dirty_row0;
static int dirty_row1;

static char status_text[VGA_COLS + 1];
static char clock_text[16];

static int focus_window = WIN_TERM;
static int mouse_x = SCREEN_W / 2;
static int mouse_y = SCREEN_H / 2;
static bool left_previous;
static bool mouse_shown;
static bool mouse_drawn;
static int mouse_draw_x;
static int mouse_draw_y;
static bool text_cursor_on;

static bool gui_running;

static void gui_leave(void);

/* entrada de teclado hacia el terminal */
#define INPUT_SIZE 256
static volatile char input_buffer[INPUT_SIZE];
static volatile int input_head;
static volatile int input_tail;

/* ------------------------------------------------------------------ */
/* Consola del terminal                                                */
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

static void mark_row(int row)
{
    if (!term_dirty) {
        term_dirty = true;
        dirty_row0 = row;
        dirty_row1 = row;
    } else {
        if (row < dirty_row0)
            dirty_row0 = row;
        if (row > dirty_row1)
            dirty_row1 = row;
    }
}

static void mark_all(void)
{
    term_dirty = true;
    dirty_row0 = 0;
    dirty_row1 = TERM_ROWS - 1;
}

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
    mark_all();
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
        mark_row(term_cy);
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

        mark_row(term_cy > 0 ? term_cy : 0);
        if (term_cy > 0)
            mark_row(term_cy - 1);
    }

    while (term_cy >= TERM_ROWS)
        term_scroll();

    mark_row(term_cy);
}

static void term_clear(console_t *con)
{
    int row;

    UNUSED(con);

    for (row = 0; row < TERM_ROWS; row++)
        clear_row(row, term_bg);

    term_cx = 0;
    term_cy = 0;
    mark_all();
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

    strncpy(status_text, text, sizeof(status_text) - 1);
    status_text[sizeof(status_text) - 1] = '\0';
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

/* ------------------------------------------------------------------ */
/* Entrada del terminal                                                */
/* ------------------------------------------------------------------ */

static void input_push(char c)
{
    int next = (input_head + 1) % INPUT_SIZE;

    cli();
    if (next != input_tail) {
        input_buffer[input_head] = c;
        input_head = next;
    }
    sti();
}

static int terminal_tryget(void)
{
    char c;
    int key;

    if (input_tail == input_head) {
        /* El puerto serie siempre alimenta la entrada: sirve para depurar
         * y para la consola remota. */
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

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */

static uint8_t focused_color(int id)
{
    return (focus_window == id) ? GFX_TITLE_ON : GFX_TITLE_OFF;
}

static void draw_window_frame(const gui_window_t *win, int id)
{
    bool focused = (focus_window == id);
    uint8_t bar = focused_color(id);

    gfx_fill(win->x + 3, win->y + 3, win->w, win->h, GFX_SHADOW);
    gfx_fill(win->x, win->y, win->w, win->h, GFX_PANEL);
    gfx_frame(win->x, win->y, win->w, win->h, GFX_BORDER);

    gfx_fill(win->x + 1, win->y + 1, win->w - 2, 18, bar);
    gfx_text(win->x + 7, win->y + 6, win->title,
             focused ? GFX_WHITE : GFX_MUTED, bar);

    /* tres puntos decorativos */
    gfx_fill(win->x + win->w - 18, win->y + 8, 3, 3, focused ? GFX_GREEN : GFX_GRAY);
    gfx_fill(win->x + win->w - 12, win->y + 8, 3, 3, focused ? GFX_CYAN : GFX_GRAY);
    gfx_fill(win->x + win->w - 6, win->y + 8, 3, 3, focused ? GFX_AMBER : GFX_GRAY);
}

static void draw_terminal_rows(int row0, int row1)
{
    int row;
    int col;

    if (row0 < 0)
        row0 = 0;
    if (row1 >= TERM_ROWS)
        row1 = TERM_ROWS - 1;

    for (row = row0; row <= row1; row++) {
        for (col = 0; col < TERM_COLS; col++) {
            int x = TERM_CX0 + col * 8;
            int y = TERM_CY0 + row * 8;
            const term_cell_t *cell = &cells[row][col];

            gfx_char(x, y, (char)cell->ch, cell->fg, cell->bg);
        }
    }
}

/* barra de progreso simple */
static void draw_bar(int x, int y, int w, int h, uint32_t used, uint32_t total,
                     uint8_t fill_color)
{
    int filled;

    gfx_fill(x, y, w, h, GFX_PANEL_ALT);
    gfx_frame(x, y, w, h, GFX_BORDER);

    if (total == 0)
        return;

    filled = (int)(((used > total ? total : used) * (uint32_t)(w - 2)) / total);
    if (filled > 0)
        gfx_fill(x + 1, y + 1, filled, h - 2, fill_color);
}

static void draw_terminal(void)
{
    const gui_window_t *win = &windows[WIN_TERM];
    int footer = TERM_FOOTER_Y;

    draw_window_frame(win, WIN_TERM);

    gfx_fill(TERM_CX0 - 4, TERM_CY0 - 4, TERM_COLS * 8 + 8,
             TERM_ROWS * 8 + 8, GFX_TERM_BG);
    draw_terminal_rows(0, TERM_ROWS - 1);

    gfx_hline(win->x + 1, footer - 5, win->w - 2, GFX_BORDER);
    gfx_text(win->x + 8, footer, "ESC: modo texto   F1/F2/F3: foco   clic: abrir archivo",
             GFX_TERM_DIM, GFX_PANEL);
}

static void format_uptime(char *buf, size_t size)
{
    uint32_t uptime = timer_uptime();

    ksnprintf(buf, size, "%02u:%02u", uptime / 60, uptime % 60);
}

static void draw_system_content(void)
{
    int x = SYS_X + 10;
    int y = SYS_Y + 30;
    char text[48];
    uint32_t total_mb = pmm_total_bytes() / (1024u * 1024u);
    uint32_t free_mb = pmm_free_bytes() / (1024u * 1024u);
    uint32_t heap_kb = heap_used_bytes() / 1024u;

    format_uptime(text, sizeof(text));
    gfx_text(x, y, "uptime", GFX_MUTED, GFX_PANEL);
    gfx_text(x + 70, y, text, GFX_TEXT, GFX_PANEL);
    y += 14;

    gfx_text(x, y, "RAM", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%u/%u MB", free_mb, total_mb);
    gfx_text(x + 70, y, text, GFX_TEXT, GFX_PANEL);
    y += 12;
    draw_bar(x, y, 156, 8, total_mb - free_mb, total_mb, GFX_GREEN);
    y += 18;

    gfx_text(x, y, "heap", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%u KB", heap_kb);
    gfx_text(x + 70, y, text, GFX_TEXT, GFX_PANEL);
    y += 12;
    draw_bar(x, y, 156, 8, heap_kb, heap_total_bytes() / 1024u, GFX_CYAN);
    y += 18;

    gfx_text(x, y, "tareas", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%d", task_count());
    gfx_text(x + 70, y, text, GFX_TEXT, GFX_PANEL);
    y += 14;

    gfx_text(x, y, "ticks", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%u", timer_ticks());
    gfx_text(x + 70, y, text, GFX_TEXT, GFX_PANEL);
    y += 14;

    gfx_text(x, y, "video", GFX_MUTED, GFX_PANEL);
    gfx_text(x + 70, y, "800x600x8", GFX_TEXT, GFX_PANEL);
}

static void draw_system(void)
{
    draw_window_frame(&windows[WIN_SYSTEM], WIN_SYSTEM);
    draw_system_content();
}

static int files_row_at(int y)
{
    int row = (y - FILES_ROW0) / FILES_ROW_H;

    if (row < 0 || row >= ramfs_count())
        return -1;

    return row;
}

static void draw_files(void)
{
    const gui_window_t *win = &windows[WIN_FILES];
    int count = ramfs_count();
    int i;
    int x = win->x + 8;
    int y = win->y + 30;

    draw_window_frame(win, WIN_FILES);

    gfx_text(x, y, "NOMBRE", GFX_MUTED, GFX_PANEL);
    gfx_text(x + 130, y, "TAMANO", GFX_MUTED, GFX_PANEL);
    y += 12;
    gfx_hline(x, y, win->w - 16, GFX_BORDER);
    y += 4;

    for (i = 0; i < count && i < 13; i++) {
        char size[16];
        uint8_t fg = (i == 0) ? GFX_GREEN : GFX_TEXT;

        gfx_text(x, y, ramfs_name_at(i), fg, GFX_PANEL);
        ksnprintf(size, sizeof(size), "%u B", ramfs_size_at(i));
        gfx_text(x + 112, y, size, GFX_MUTED, GFX_PANEL);
        y += 14;
    }

    gfx_text(x, win->y + win->h - 16, "clic: abrir archivo",
             GFX_TERM_DIM, GFX_PANEL);
}

static void draw_topbar(void)
{
    int x = 140;

    gfx_fill(0, 0, SCREEN_W, TOPBAR_H, GFX_PANEL);
    gfx_hline(0, TOPBAR_H - 1, SCREEN_W, GFX_BORDER);

    gfx_fill(8, 5, 18, 18, GFX_DARK_GREEN);
    gfx_text(13, 10, "S", GFX_GREEN, GFX_DARK_GREEN);

    gfx_text(32, 10, "SaveiroOS", GFX_WHITE, GFX_PANEL);
    gfx_text(112, 10, "0.1", GFX_MUTED, GFX_PANEL);

    /* botones de ventanas */
    {
        static const char *names[WIN_COUNT] = { "Terminal", "Sistema", "Archivos" };
        int i;

        for (i = 0; i < WIN_COUNT; i++) {
            int width = (int)strlen(names[i]) * 8 + 14;
            bool active = (focus_window == i);

            gfx_fill(x, 5, width, 18, active ? GFX_DARK_GREEN : GFX_PANEL_ALT);
            gfx_frame(x, 5, width, 18, active ? GFX_GREEN : GFX_BORDER);
            gfx_text(x + 7, 10, names[i], active ? GFX_GREEN : GFX_MUTED,
                     active ? GFX_DARK_GREEN : GFX_PANEL_ALT);

            x += width + 6;
        }
    }

    /* reloj y memoria */
    gfx_text(600, 10, "RAM", GFX_MUTED, GFX_PANEL);
    {
        char text[32];

        ksnprintf(text, sizeof(text), "%u/%u MB",
                  pmm_free_bytes() / (1024u * 1024u),
                  pmm_total_bytes() / (1024u * 1024u));
        gfx_text(632, 10, text, GFX_TEXT, GFX_PANEL);
    }

    gfx_text(728, 10, clock_text, GFX_GREEN, GFX_PANEL);
}

static void draw_bottombar(void)
{
    gfx_fill(0, BOTTOM_Y, SCREEN_W, BOTTOM_H, GFX_PANEL);
    gfx_hline(0, BOTTOM_Y, SCREEN_W, GFX_BORDER);

    if (status_text[0])
        gfx_text(8, BOTTOM_Y + 10, status_text, GFX_MUTED, GFX_PANEL);

    gfx_text(626, BOTTOM_Y + 10, "F1/F2/F3 ventanas", GFX_TERM_DIM, GFX_PANEL);
}

static void draw_desktop(void)
{
    int x;
    int y;

    gfx_fill(0, 0, SCREEN_W, SCREEN_H, GFX_BG);

    for (y = TOPBAR_H + 2; y < BOTTOM_Y; y += 4) {
        for (x = (y / 4) % 2 ? 2 : 0; x < SCREEN_W; x += 4)
            gfx_pixel(x, y, GFX_DESKTOP);
    }

    /* marca de agua */
    gfx_text(280, 300, "S A V E I R O", GFX_DESKTOP, GFX_BG);
}


static void cursor_hide(void)
{
    if (!mouse_drawn)
        return;

    gfx_restore(mouse_draw_x, mouse_draw_y, 10, 16);
    mouse_drawn = false;
}

static void cursor_show(void)
{
    if (!mouse_shown || mouse_drawn)
        return;

    gfx_cursor_draw(mouse_x, mouse_y);
    mouse_draw_x = mouse_x;
    mouse_draw_y = mouse_y;
    mouse_drawn = true;
}

static void render_all(void)
{
    mouse_drawn = false;
    draw_desktop();
    draw_topbar();
    draw_terminal();
    draw_system();
    draw_files();
    draw_bottombar();
    gfx_flush_all();

    cursor_show();
}

static void render_terminal_dirty(void)
{
    int rows;
    int y;

    if (!term_dirty)
        return;

    cursor_hide();

    draw_terminal_rows(dirty_row0, dirty_row1);

    rows = dirty_row1 - dirty_row0 + 1;
    y = TERM_CY0 + dirty_row0 * 8;
    gfx_flush(TERM_CX0 - 2, y - 2, TERM_COLS * 8 + 4, rows * 8 + 4);

    term_dirty = false;

    cursor_show();
}

/* ------------------------------------------------------------------ */
/* Interacción                                                         */
/* ------------------------------------------------------------------ */

static bool inside(int x, int y, int w, int h, int px, int py)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void focus_window_at(int px, int py)
{
    int i;

    for (i = WIN_COUNT - 1; i >= 0; i--) {
        if (inside(windows[i].x, windows[i].y, windows[i].w, windows[i].h, px, py)) {
            if (focus_window != i) {
                focus_window = i;
                render_all();
            }
            return;
        }
    }
}

static void queue_command(const char *command)
{
    const char *p = command;

    while (*p)
        input_push(*p++);
    input_push('\n');

    focus_window = WIN_TERM;
    render_all();
}

static void handle_click(int px, int py)
{
    int i;
    int x = 140;

    /* botones de la barra superior */
    if (py < TOPBAR_H) {
        static const char *names[WIN_COUNT] = { "Terminal", "Sistema", "Archivos" };

        for (i = 0; i < WIN_COUNT; i++) {
            int width = (int)strlen(names[i]) * 8 + 14;

            if (inside(x, 5, width, 18, px, py)) {
                focus_window = i;
                render_all();
                return;
            }
            x += width + 6;
        }
        return;
    }

    /* filas del panel de archivos */
    if (inside(FILES_X, FILES_ROW0, FILES_W - 16, 13 * FILES_ROW_H, px, py)) {
        int row = files_row_at(py);

        if (row >= 0) {
            char command[48];
            const char *name = ramfs_name_at(row);

            ksnprintf(command, sizeof(command), "cat %s", name);
            queue_command(command);
            return;
        }
    }

    focus_window_at(px, py);
}

static void handle_mouse(void)
{
    mouse_event_t event;
    bool pressed;

    if (!mouse_read(&event))
        return;

    mouse_x += event.dx;
    mouse_y += event.dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x > SCREEN_W - 8) mouse_x = SCREEN_W - 8;
    if (mouse_y > SCREEN_H - 12) mouse_y = SCREEN_H - 12;

    pressed = event.left && !left_previous;
    left_previous = event.left;

    if (pressed)
        handle_click(mouse_x, mouse_y);
}

static void handle_keys(void)
{
    int key;

    while ((key = keyboard_poll()) >= 0)
        input_push((char)key);

    while ((key = keyboard_poll_special()) >= 0) {
        if (key == 27) {
            gui_leave();
            return;
        }

        if (key >= 1 && key <= 3) {
            focus_window = key - 1;
            render_all();
        }
    }
}

static void update_clock(void)
{
    rtc_time_t now;

    rtc_read(&now);
    ksnprintf(clock_text, sizeof(clock_text), "%02u:%02u:%02u",
              now.hour, now.minute, now.second);

    cursor_hide();

    /* reloj */
    gfx_fill(728, 10, 64, 10, GFX_PANEL);
    gfx_text(728, 10, clock_text, GFX_GREEN, GFX_PANEL);
    gfx_flush(728, 8, 66, 14);

    /* panel del sistema */
    gfx_fill(SYS_X + 1, SYS_Y + 19, SYS_W - 2, SYS_H - 20, GFX_PANEL);
    draw_system_content();
    gfx_flush(SYS_X + 1, SYS_Y + 19, SYS_W - 2, SYS_H - 20);

    cursor_show();
}


static void text_cursor(bool on)
{
    int x = TERM_CX0 + (term_cx < TERM_COLS ? term_cx : 0) * 8;
    int y = TERM_CY0 + (term_cy < TERM_ROWS ? term_cy : 0) * 8 + 6;

    if (on)
        gfx_fill(x, y, 6, 2, GFX_GREEN);
    else
        gfx_restore(x, y, 8, 4);
}

static void gui_leave(void)
{
    gui_running = false;

    gfx_shutdown();
    console_use(vga_console());

    kprintf("\nSaliste del escritorio. Modo texto de nuevo.\n");

    task_kill(task_current()->id);

    for (;;)
        hlt();
}

static void gui_task(void *arg)
{
    uint32_t last_clock = 0;
    uint32_t last_blink = 0;

    UNUSED(arg);

    for (;;) {
        handle_mouse();
        handle_keys();

        if (term_dirty)
            render_terminal_dirty();

        /* parpadeo del cursor de texto */
        if (timer_ticks() - last_blink >= 25) {
            last_blink = timer_ticks();
            text_cursor_on = !text_cursor_on;

            cursor_hide();

            text_cursor(text_cursor_on);

            cursor_show();
        }

        if (timer_ticks() - last_clock >= 50) {
            last_clock = timer_ticks();
            update_clock();
            draw_bottombar();
            gfx_flush(0, BOTTOM_Y, SCREEN_W, BOTTOM_H);
        }

        /* cursor del ratón */
        cursor_hide();
        cursor_show();

        hlt();
    }
}

/* ------------------------------------------------------------------ */
/* Arranque                                                            */
/* ------------------------------------------------------------------ */

void gui_start(void)
{
    int row;

    input_head = 0;
    input_tail = 0;
    term_cx = 0;
    term_cy = 0;
    term_dirty = false;
    status_text[0] = '\0';
    clock_text[0] = '\0';
    mouse_shown = true;
    mouse_drawn = false;
    focus_window = WIN_TERM;
    mouse_x = SCREEN_W / 2;
    mouse_y = SCREEN_H / 2;
    left_previous = true;   /* evita un clic fantasma al entrar */

    for (row = 0; row < TERM_ROWS; row++)
        clear_row(row, GFX_TERM_BG);

    console_use(terminal_console());
    con_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    input_set_source(terminal_tryget);

    mouse_init();
    pic_unmask(2);
    pic_unmask(12);

    update_clock();
    render_all();

    gui_running = true;
    task_create("gui", gui_task, NULL);
}

int gui_is_active(void)
{
    return gui_running;
}
