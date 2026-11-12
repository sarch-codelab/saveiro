/* Aplicaciones del escritorio: archivos, editor, calculadora, sistema,
 * acerca de y los iconos del escritorio.
 */

#include <types.h>
#include <gfx.h>
#include <console.h>
#include <ramfs.h>
#include <pit.h>
#include <pmm.h>
#include <heap.h>
#include <task.h>
#include <rtc.h>
#include <string.h>

#include "gui_internal.h"

/* ------------------------------------------------------------------ */
/* Iconos del escritorio                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *label;
    const char *symbol;
    void (*open)(void);
} icon_t;

static const icon_t icons[6] = {
    { "Term", "T:", app_terminal_open },
    { "Arch", "[]", app_files_open },
    { "Edit", "E:", app_editor_open_none },
    { "Calc", "%%", app_calc_open },
    { "Sist", "i:", app_system_open },
    { "Info", "?",  app_about_open },
};

#define ICON_X 16
#define ICON_Y 22
#define ICON_W 52
#define ICON_H 62

static int icon_at(int x, int y)
{
    int i;

    for (i = 0; i < 6; i++) {
        int iy = ICON_Y + i * (ICON_H + 10);

        if (wm_hit(ICON_X, iy, ICON_W, ICON_H, x, y))
            return i;
    }

    return -1;
}

void apps_draw_desktop(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        int y = ICON_Y + i * (ICON_H + 10);
        int symbol_w = wm_text_width(icons[i].symbol);
        int label_w = wm_text_width(icons[i].label);

        gfx_fill(ICON_X, y, ICON_W, 44, GFX_PANEL);
        gfx_frame(ICON_X, y, ICON_W, 44, GFX_BORDER_LT);
        gfx_fill(ICON_X + 1, y + 1, ICON_W - 2, 3, GFX_DARK_GREEN);

        gfx_text(ICON_X + (ICON_W - symbol_w) / 2, y + 18, icons[i].symbol,
                 GFX_GREEN, GFX_PANEL);
        gfx_text(ICON_X + (ICON_W - label_w) / 2, y + 49, icons[i].label,
                 GFX_TEXT, GFX_DESKTOP);
    }
}

bool apps_desktop_click(int x, int y)
{
    int index = icon_at(x, y);

    if (index < 0)
        return false;

    icons[index].open();
    return true;
}

/* ------------------------------------------------------------------ */
/* Archivos                                                            */
/* ------------------------------------------------------------------ */

#define FILE_ROW_H 16
#define FILES_BUTTON_H 20

typedef struct {
    int selected;
} files_data_t;

static files_data_t files_data;
static wm_window_t *files_window;

static int files_rows(wm_window_t *win)
{
    int rows = (wm_content_h(win) - 46 - FILES_BUTTON_H - 8) / FILE_ROW_H;

    if (rows < 1)
        rows = 1;

    return rows;
}

static int files_index_at(wm_window_t *win, int y)
{
    int top = wm_content_y(win) + 24;
    int row = (y - top) / FILE_ROW_H;

    if (row < 0 || row >= ramfs_count())
        return -1;

    return row;
}

static void files_draw(wm_window_t *win)
{
    int x = wm_content_x(win);
    int y = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    int count = ramfs_count();
    int rows = files_rows(win);
    int i;
    int button_y = y + h - FILES_BUTTON_H - 4;
    int button_w = (w - 16) / 3;

    gfx_fill(x, y, w, h, GFX_PANEL);
    gfx_text(x + 8, y + 8, "NOMBRE", GFX_MUTED, GFX_PANEL);
    gfx_text(x + w - 8 - 8 * 8, y + 8, "TAMANO", GFX_MUTED, GFX_PANEL);
    gfx_hline(x + 6, y + 20, w - 12, GFX_BORDER);

    for (i = 0; i < count && i < rows; i++) {
        int ry = y + 24 + i * FILE_ROW_H;
        bool selected = (i == files_data.selected);
        char size[16];
        uint8_t bg = selected ? GFX_DARK_GREEN : GFX_PANEL;
        uint8_t fg = selected ? GFX_GREEN : GFX_TEXT;

        if (selected)
            gfx_fill(x + 4, ry - 2, w - 8, FILE_ROW_H, bg);

        gfx_text(x + 8, ry, ramfs_name_at(i), fg, bg);

        ksnprintf(size, sizeof(size), "%u B", ramfs_size_at(i));
        gfx_text(x + w - 8 - wm_text_width(size), ry, size,
                 selected ? GFX_GREEN : GFX_TERM_DIM, bg);
    }

    wm_button(x + 6, button_y, button_w, FILES_BUTTON_H, "Abrir", false);
    wm_button(x + 10 + button_w, button_y, button_w, FILES_BUTTON_H, "Nuevo", false);
    wm_button(x + 14 + button_w * 2, button_y, button_w, FILES_BUTTON_H, "Borrar", false);
}

static void files_click(wm_window_t *win, int x, int y, bool double_click)
{
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    int left = wm_content_x(win);
    int top = wm_content_y(win);
    int button_y = top + h - FILES_BUTTON_H - 4;
    int button_w = (w - 16) / 3;
    int index = files_index_at(win, y);

    if (index >= 0) {
        files_data.selected = index;

        if (double_click)
            app_editor_open(ramfs_name_at(index));

        wm_window_damage(win);
        return;
    }

    if (y >= button_y && y < button_y + FILES_BUTTON_H) {
        if (wm_hit(left + 6, button_y, button_w, FILES_BUTTON_H, x, y)) {
            if (ramfs_count() > 0)
                app_editor_open(ramfs_name_at(files_data.selected));
            return;
        }

        if (wm_hit(left + 10 + button_w, button_y, button_w, FILES_BUTTON_H, x, y)) {
            char name[16];
            int i;

            for (i = 1; i < 100; i++) {
                ksnprintf(name, sizeof(name), "nuevo%u", (unsigned)i);
                if (!ramfs_exists(name))
                    break;
            }

            if (ramfs_create(name) >= 0) {
                files_data.selected = ramfs_count() - 1;
                wm_window_damage(win);
                app_editor_open(name);
            }
            return;
        }

        if (wm_hit(left + 14 + button_w * 2, button_y, button_w, FILES_BUTTON_H, x, y)) {
            if (ramfs_count() > 0) {
                ramfs_unlink(ramfs_name_at(files_data.selected));
                if (files_data.selected >= ramfs_count() && files_data.selected > 0)
                    files_data.selected--;
                wm_invalidate();
            }
            return;
        }
    }
}

void app_files_open(void)
{
    if (files_window) {
        files_window->visible = true;
        wm_focus(files_window);
        return;
    }

    files_window = wm_create("Archivos", 470, 70, 310, 280);
    if (!files_window)
        return;

    files_window->data = &files_data;
    files_window->draw = files_draw;
    files_window->click = files_click;
}

/* ------------------------------------------------------------------ */
/* Editor                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[24];
    char text[RAMFS_MAX_SIZE];
    int  len;
    int  caret;
    bool modified;
} editor_data_t;

static editor_data_t editor_data;
static wm_window_t *editor_window;

static void editor_load(const char *name)
{
    int n;

    memset(editor_data.name, 0, sizeof(editor_data.name));
    strncpy(editor_data.name, name, sizeof(editor_data.name) - 1);
    editor_data.len = 0;
    editor_data.caret = 0;
    editor_data.modified = false;

    n = ramfs_read(editor_data.name, editor_data.text, sizeof(editor_data.text) - 1);
    if (n > 0) {
        editor_data.len = n;
        editor_data.caret = n;
    }
}

static void editor_save(void)
{
    ramfs_write(editor_data.name, editor_data.text, (size_t)editor_data.len, false);
    editor_data.modified = false;
    wm_invalidate();
}

static void editor_draw(wm_window_t *win)
{
    int x = wm_content_x(win);
    int y = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    int status_y = y + h - 14;
    int cols = (w - 12) / 8;
    int rows = (h - 24) / 8;
    int start = 0;
    int line = 0;
    char info[48];

    gfx_fill(x, y, w, h, GFX_TERM_BG);
    gfx_fill(x, y, w, 18, GFX_PANEL);
    gfx_text(x + 6, y + 5, editor_data.name, GFX_GREEN, GFX_PANEL);
    wm_button(x + w - 78, y + 2, 72, 14, "Guardar", false);

    while (start <= editor_data.len && line < rows) {
        int end = start;
        int col;

        while (end < editor_data.len && end - start < cols && editor_data.text[end] != '\n')
            end++;

        for (col = start; col < end; col++)
            gfx_char(x + 6 + (col - start) * 8, y + 20 + line * 8,
                     editor_data.text[col], GFX_TERM_TEXT, GFX_TERM_BG);

        if (editor_data.caret >= start && editor_data.caret <= end) {
            int cx = x + 6 + (editor_data.caret - start) * 8;
            int cy = y + 20 + line * 8 + 6;

            gfx_fill(cx, cy, 6, 2, GFX_GREEN);
        }

        if (end == start) {
            if (start >= editor_data.len)
                break;
            start++;
            line++;
            continue;
        }

        start = end + (editor_data.text[end] == '\n' ? 1 : 0);
        line++;
    }

    gfx_hline(x + 4, status_y - 3, w - 8, GFX_BORDER);
    if (editor_data.modified)
        ksnprintf(info, sizeof(info), "%d bytes - sin guardar", editor_data.len);
    else
        ksnprintf(info, sizeof(info), "%d bytes - guardado", editor_data.len);

    gfx_text(x + 6, status_y, info, GFX_TERM_DIM, GFX_TERM_BG);
}

static void editor_key(wm_window_t *win, int key)
{
    if (key == '\n' || key == '\r') {
        key = '\n';
    } else if (key == '\b' || key == 127) {
        if (editor_data.caret > 0) {
            int i;

            for (i = editor_data.caret - 1; i < editor_data.len - 1; i++)
                editor_data.text[i] = editor_data.text[i + 1];

            editor_data.caret--;
            editor_data.len--;
            editor_data.modified = true;
        }
        wm_window_damage(win);
        return;
    } else if (key == 19) {         /* Ctrl+S */
        editor_save();
        wm_window_damage(win);
        return;
    } else if (key < 32 || key > 126) {
        return;
    }

    if (editor_data.len >= (int)sizeof(editor_data.text) - 2)
        return;

    {
        int i;

        for (i = editor_data.len; i > editor_data.caret; i--)
            editor_data.text[i] = editor_data.text[i - 1];

        editor_data.text[editor_data.caret] = (char)key;
        editor_data.caret++;
        editor_data.len++;
        editor_data.text[editor_data.len] = '\0';
        editor_data.modified = true;
    }

    wm_window_damage(win);
}

static void editor_click(wm_window_t *win, int x, int y, bool double_click)
{
    int w = wm_content_w(win);
    int top = wm_content_y(win);
    int left = wm_content_x(win);

    UNUSED(double_click);

    if (wm_hit(left + w - 78, top + 2, 72, 14, x, y)) {
        editor_save();
        wm_window_damage(win);
        return;
    }

    wm_window_damage(win);
}

static void editor_close(wm_window_t *win)
{
    UNUSED(win);

    if (editor_data.modified)
        editor_save();
}

void app_editor_open(const char *name)
{
    if (!name || !*name)
        name = ramfs_count() > 0 ? ramfs_name_at(0) : "notas";

    if (!editor_window) {
        editor_window = wm_create("Editor", 190, 80, 500, 340);
        if (!editor_window)
            return;

        editor_window->draw = editor_draw;
        editor_window->key = editor_key;
        editor_window->click = editor_click;
        editor_window->close = editor_close;
    }

    editor_load(name);
    editor_window->visible = true;
    wm_focus(editor_window);
    wm_invalidate();
}

void app_editor_open_none(void)
{
    app_editor_open(NULL);
}

/* ------------------------------------------------------------------ */
/* Calculadora                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    int32_t accumulator;
    int32_t current;
    char    op;
    bool    entering;
    bool    error;
} calc_data_t;

static calc_data_t calc;
static wm_window_t *calc_window;

#define CALC_BUTTONS 16
static const char calc_labels[CALC_BUTTONS] = {
    '7', '8', '9', '/',
    '4', '5', '6', '*',
    '1', '2', '3', '-',
    '0', '.', '=', '+'
};

static void calc_reset(void)
{
    calc.accumulator = 0;
    calc.current = 0;
    calc.op = 0;
    calc.entering = false;
    calc.error = false;
}

static void calc_draw(wm_window_t *win)
{
    int x = wm_content_x(win);
    int y = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    int cell_w = (w - 10) / 4;
    int cell_h = (h - 54) / 4;
    char text[24];
    int i;

    gfx_fill(x, y, w, h, GFX_PANEL);

    gfx_fill(x + 6, y + 6, w - 12, 24, GFX_TERM_BG);
    gfx_frame(x + 6, y + 6, w - 12, 24, GFX_BORDER);

    if (calc.error)
        ksnprintf(text, sizeof(text), "error");
    else
        ksnprintf(text, sizeof(text), "%d", (int)calc.current);

    gfx_text(x + w - 12 - wm_text_width(text), y + 14, text, GFX_GREEN, GFX_TERM_BG);

    for (i = 0; i < CALC_BUTTONS; i++) {
        int col = i % 4;
        int row = i / 4;
        int bx = x + 6 + col * cell_w;
        int by = y + 40 + row * cell_h;
        char label[2];

        label[0] = calc_labels[i];
        label[1] = '\0';

        wm_button(bx, by, cell_w - 4, cell_h - 4, label, false);
    }
}

static void calc_press(char key)
{
    if (key == 'C') {
        calc_reset();
        return;
    }

    if (key >= '0' && key <= '9') {
        if (!calc.entering)
            calc.current = 0;
        calc.current = calc.current * 10 + (key - '0');
        calc.entering = true;
        calc.error = false;
        return;
    }

    if (key == '+' || key == '-' || key == '*' || key == '/') {
        calc.accumulator = calc.current;
        calc.op = key;
        calc.entering = false;
        return;
    }

    if (key == '=' || key == '\n' || key == '\r') {
        int32_t a = calc.accumulator;
        int32_t b = calc.current;

        switch (calc.op) {
        case '+': a = a + b; break;
        case '-': a = a - b; break;
        case '*': a = a * b; break;
        case '/':
            if (b == 0) {
                calc.error = true;
                a = 0;
            } else {
                a = a / b;
            }
            break;
        default:
            a = b;
            break;
        }

        calc.current = a;
        calc.accumulator = a;
        calc.op = 0;
    }
}

static void calc_click(wm_window_t *win, int x, int y, bool double_click)
{
    int cx = wm_content_x(win);
    int cy = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    int cell_w = (w - 10) / 4;
    int cell_h = (h - 54) / 4;
    int i;

    UNUSED(double_click);

    for (i = 0; i < CALC_BUTTONS; i++) {
        int col = i % 4;
        int row = i / 4;
        int bx = cx + 6 + col * cell_w;
        int by = cy + 40 + row * cell_h;

        if (wm_hit(bx, by, cell_w - 4, cell_h - 4, x, y)) {
            calc_press(calc_labels[i]);
            wm_window_damage(win);
            return;
        }
    }
}

static void calc_key(wm_window_t *win, int key)
{
    calc_press((char)key);
    wm_window_damage(win);
}

void app_calc_open(void)
{
    if (calc_window) {
        calc_window->visible = true;
        wm_focus(calc_window);
        return;
    }

    calc_window = wm_create("Calculadora", 520, 150, 230, 300);
    if (!calc_window)
        return;

    calc_window->min_w = 180;
    calc_window->min_h = 200;
    calc_window->draw = calc_draw;
    calc_window->click = calc_click;
    calc_window->key = calc_key;
    calc_reset();
}

/* ------------------------------------------------------------------ */
/* Sistema                                                             */
/* ------------------------------------------------------------------ */

static wm_window_t *system_window;

static void system_draw(wm_window_t *win)
{
    int x = wm_content_x(win);
    int y = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);
    char text[48];
    uint32_t total_mb = pmm_total_bytes() / (1024u * 1024u);
    uint32_t free_mb = pmm_free_bytes() / (1024u * 1024u);
    uint32_t heap_kb = heap_used_bytes() / 1024u;
    uint32_t uptime = timer_uptime();

    gfx_fill(x, y, w, h, GFX_PANEL);
    gfx_text(x + 10, y + 10, "Recursos del sistema", GFX_GREEN, GFX_PANEL);
    gfx_hline(x + 8, y + 24, w - 16, GFX_BORDER);

    gfx_text(x + 10, y + 34, "RAM libre", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%u de %u MB", free_mb, total_mb);
    gfx_text(x + w - 10 - wm_text_width(text), y + 34, text, GFX_TEXT, GFX_PANEL);
    wm_bar(x + 10, y + 48, w - 20, 10, total_mb - free_mb, total_mb, GFX_GREEN);

    gfx_text(x + 10, y + 70, "Heap", GFX_MUTED, GFX_PANEL);
    ksnprintf(text, sizeof(text), "%u KB", heap_kb);
    gfx_text(x + w - 10 - wm_text_width(text), y + 70, text, GFX_TEXT, GFX_PANEL);
    wm_bar(x + 10, y + 84, w - 20, 10, heap_kb, heap_total_bytes() / 1024u, GFX_CYAN);

    ksnprintf(text, sizeof(text), "tareas: %d", task_count());
    gfx_text(x + 10, y + 108, text, GFX_TEXT, GFX_PANEL);

    ksnprintf(text, sizeof(text), "ticks: %u", timer_ticks());
    gfx_text(x + 10, y + 124, text, GFX_TEXT, GFX_PANEL);

    ksnprintf(text, sizeof(text), "uptime: %u:%02u", uptime / 60, uptime % 60);
    gfx_text(x + 10, y + 140, text, GFX_TEXT, GFX_PANEL);

    gfx_text(x + 10, y + 164, "Video: 800x600, 8 bpp", GFX_TERM_DIM, GFX_PANEL);
    gfx_text(x + 10, y + 178, "RAMFS: archivos en memoria", GFX_TERM_DIM, GFX_PANEL);
}

static void system_click(wm_window_t *win, int x, int y, bool double_click)
{
    UNUSED(x);
    UNUSED(y);
    UNUSED(double_click);

    wm_window_damage(win);
}

void app_system_open(void)
{
    if (system_window) {
        system_window->visible = true;
        wm_focus(system_window);
        return;
    }

    system_window = wm_create("Sistema", 470, 360, 310, 200);
    if (!system_window)
        return;

    system_window->min_h = 200;
    system_window->draw = system_draw;
    system_window->click = system_click;
}

/* ------------------------------------------------------------------ */
/* Acerca de                                                           */
/* ------------------------------------------------------------------ */

static wm_window_t *about_window;

static void about_draw(wm_window_t *win)
{
    int x = wm_content_x(win);
    int y = wm_content_y(win);
    int w = wm_content_w(win);
    int h = wm_content_h(win);

    gfx_fill(x, y, w, h, GFX_PANEL);

    gfx_text(x + 16, y + 18, "S A V E I R O   O S", GFX_GREEN, GFX_PANEL);
    gfx_text(x + 16, y + 36, "version 0.2", GFX_MUTED, GFX_PANEL);

    gfx_hline(x + 16, y + 54, w - 32, GFX_BORDER);

    gfx_text(x + 16, y + 66, "Sistema operativo x86 de 32 bits", GFX_TEXT, GFX_PANEL);
    gfx_text(x + 16, y + 82, "escrito desde cero, sin libc.", GFX_TEXT, GFX_PANEL);
    gfx_text(x + 16, y + 104, "Bootloader propio, kernel freestanding,", GFX_MUTED, GFX_PANEL);
    gfx_text(x + 16, y + 118, "paginacion, multitarea y escritorio.", GFX_MUTED, GFX_PANEL);
    gfx_text(x + 16, y + 140, "Arrastra las ventanas por la barra de titulo.", GFX_TERM_DIM, GFX_PANEL);
    gfx_text(x + 16, y + 154, "Doble clic en Archivos para editar.", GFX_TERM_DIM, GFX_PANEL);

    gfx_text(x + w - 16 - 22 * 8, y + h - 18, "Hecho con C y ensamblador", GFX_TERM_DIM, GFX_PANEL);
}

void app_about_open(void)
{
    if (about_window) {
        about_window->visible = true;
        wm_focus(about_window);
        return;
    }

    about_window = wm_create("Acerca de SaveiroOS", 160, 140, 440, 220);
    if (!about_window)
        return;

    about_window->draw = about_draw;
}
