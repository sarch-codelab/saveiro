/* Gestor de ventanas del escritorio de SaveiroOS.
 *
 * Un compositor minimo: todo se dibuja en el back buffer con un rectangulo de
 * recorte y solo se vuelca a la tarjeta la zona que cambia. Las ventanas se
 * apilan, se arrastran por la barra de titulo, se redimensionan por la esquina
 * inferior derecha y se minimizan, maximizan o cierran con sus botones.
 */

#include <types.h>
#include <console.h>
#include <io.h>
#include <gfx.h>
#include <mouse.h>
#include <keyboard.h>
#include <task.h>
#include <pit.h>
#include <rtc.h>
#include <string.h>
#include <pmm.h>
#include <heap.h>

#include "gui_internal.h"

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

#define MAX_DAMAGE 16

typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

static wm_window_t pool[WM_MAX_WINDOWS];
static bool pool_used[WM_MAX_WINDOWS];
static wm_window_t *stack[WM_MAX_WINDOWS];
static int stack_count;
static wm_window_t *focused;
static int next_id = 1;

static rect_t damage[MAX_DAMAGE];
static int damage_count;
static bool damage_all;

static bool wm_running;

/* escritorio */
static int mouse_x = WM_SCREEN_W / 2;
static int mouse_y = WM_SCREEN_H / 2;
static bool mouse_left_before;
static bool cursor_up;
static int cursor_x;
static int cursor_y;

/*arrastre y redimensionado */
static uint32_t last_click_tick;
static int last_click_x;
static int last_click_y;

static int drag_mode;      /* 0 nada, 1 mover, 2 redimensionar */
static int drag_off_x;
static int drag_off_y;
static wm_window_t *drag_win;

/* menu de inicio */
static bool start_open;

typedef struct {
    const char *name;
    void (*open)(void);
} start_item_t;

static const start_item_t start_items[6];

#define START_X 4
#define START_Y (WM_TASKBAR_Y + 4)
#define START_W 72
#define START_H 22

#define MENU_W      190
#define MENU_ITEM_H 24
#define MENU_ITEMS  6
#define MENU_X      4
#define MENU_H      (22 + MENU_ITEMS * MENU_ITEM_H + 8)
#define MENU_Y      (WM_TASKBAR_Y - MENU_H)

static rect_t clip_rect;
static char clock_buf[12];

static void wm_task(void *arg);
static void draw_start_menu(void);
static void update_clock(void);

/* ------------------------------------------------------------------ */
/* Daño                                                                */
/* ------------------------------------------------------------------ */

void wm_damage(int x, int y, int w, int h)
{
    if (damage_all || w <= 0 || h <= 0)
        return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WM_SCREEN_W) w = WM_SCREEN_W - x;
    if (y + h > WM_SCREEN_H) h = WM_SCREEN_H - y;
    if (w <= 0 || h <= 0)
        return;

    if (damage_count >= MAX_DAMAGE) {
        damage_all = true;
        damage_count = 0;
        return;
    }

    damage[damage_count].x = x;
    damage[damage_count].y = y;
    damage[damage_count].w = w;
    damage[damage_count].h = h;
    damage_count++;
}

void wm_invalidate(void)
{
    damage_all = true;
    damage_count = 0;
}

void wm_window_damage(wm_window_t *win)
{
    if (!win || !win->visible)
        return;

    wm_damage(win->x, win->y, win->w + 6, win->h + 6);
}

void wm_content_damage(wm_window_t *win, int x, int y, int w, int h)
{
    if (!win || !win->visible)
        return;

    wm_damage(wm_content_x(win) + x, wm_content_y(win) + y, w, h);
}

void wm_close_damage(void)
{
    wm_invalidate();
}

/* ------------------------------------------------------------------ */
/* Geometria                                                           */
/* ------------------------------------------------------------------ */

int wm_content_x(wm_window_t *win) { return win->x + 1; }
int wm_content_y(wm_window_t *win) { return win->y + WM_TITLE_H; }
int wm_content_w(wm_window_t *win) { return win->w - 2; }
int wm_content_h(wm_window_t *win) { return win->h - WM_TITLE_H - 1; }

void wm_clip_rect(int *x, int *y, int *w, int *h)
{
    if (x) *x = clip_rect.x;
    if (y) *y = clip_rect.y;
    if (w) *w = clip_rect.w;
    if (h) *h = clip_rect.h;
}

bool wm_hit(int x, int y, int w, int h, int px, int py)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

int wm_text_width(const char *text)
{
    return (int)strlen(text) * 8;
}

void wm_label(int x, int y, const char *text, uint8_t fg, uint8_t bg)
{
    gfx_text(x, y, text, fg, bg);
}

void wm_button(int x, int y, int w, int h, const char *label, bool active)
{
    uint8_t bg = active ? GFX_DARK_GREEN : GFX_PANEL_ALT;
    uint8_t fg = active ? GFX_GREEN : GFX_TEXT;
    int width = wm_text_width(label);

    gfx_fill(x, y, w, h, bg);
    gfx_frame(x, y, w, h, active ? GFX_GREEN : GFX_BORDER);
    gfx_text(x + (w - width) / 2, y + (h - 8) / 2, label, fg, bg);
}

void wm_bar(int x, int y, int w, int h, uint32_t used, uint32_t total, uint8_t color)
{
    int filled;

    gfx_fill(x, y, w, h, GFX_PANEL_ALT);
    gfx_frame(x, y, w, h, GFX_BORDER);

    if (!total || w <= 2)
        return;

    if (used > total)
        used = total;

    filled = (int)(used * (uint32_t)(w - 2) / total);
    if (filled > 0)
        gfx_fill(x + 1, y + 1, filled, h - 2, color);
}

int wm_window_count(void) { return stack_count; }

wm_window_t *wm_window_at(int index)
{
    if (index < 0 || index >= stack_count)
        return NULL;

    return stack[index];
}

wm_window_t *wm_focused(void) { return focused; }

/* ------------------------------------------------------------------ */
/* Ventanas                                                            */
/* ------------------------------------------------------------------ */

static void stack_remove(wm_window_t *win)
{
    int i;
    int j;

    for (i = 0; i < stack_count; i++) {
        if (stack[i] == win) {
            for (j = i; j < stack_count - 1; j++)
                stack[j] = stack[j + 1];
            stack_count--;
            return;
        }
    }
}

static void stack_push(wm_window_t *win)
{
    stack_remove(win);

    if (stack_count < WM_MAX_WINDOWS)
        stack[stack_count++] = win;
}

wm_window_t *wm_create(const char *title, int x, int y, int w, int h)
{
    int slot;
    wm_window_t *win = NULL;

    for (slot = 0; slot < WM_MAX_WINDOWS; slot++) {
        if (!pool_used[slot]) {
            pool_used[slot] = true;
            win = &pool[slot];
            break;
        }
    }

    if (!win)
        return NULL;

    memset(win, 0, sizeof(*win));
    win->id = next_id++;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->visible = true;
    win->resizable = true;
    win->min_w = 140;
    win->min_h = 80;

    stack_push(win);
    focused = win;
    wm_invalidate();

    return win;
}

void wm_destroy(wm_window_t *win)
{
    int slot;

    if (!win)
        return;

    if (win->close)
        win->close(win);

    stack_remove(win);

    for (slot = 0; slot < WM_MAX_WINDOWS; slot++) {
        if (&pool[slot] == win) {
            pool_used[slot] = false;
            break;
        }
    }

    focused = stack_count ? stack[stack_count - 1] : NULL;
    wm_invalidate();
}

void wm_focus(wm_window_t *win)
{
    if (!win || focused == win)
        return;

    focused = win;
    stack_push(win);
    wm_invalidate();
}

void wm_set_title(wm_window_t *win, const char *title)
{
    if (!win)
        return;

    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
    wm_window_damage(win);
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */

static void draw_desktop_background(void)
{
    int x;
    int y;

    gfx_fill(0, 0, WM_SCREEN_W, WM_TASKBAR_Y, GFX_DESKTOP);

    for (y = 8; y < WM_TASKBAR_Y; y += 16) {
        for (x = 8; x < WM_SCREEN_W; x += 16)
            gfx_pixel(x, y, GFX_BG);
    }

    gfx_text(WM_SCREEN_W - 8 - 11 * 8, WM_TASKBAR_Y - 30,
             "S A V E I R O", GFX_BG, GFX_DESKTOP);
}

static void title_button(int x, int y, int kind, bool active)
{
    uint8_t bg = active ? GFX_TITLE_ON : GFX_TITLE_OFF;
    uint8_t fg = active ? GFX_MUTED : GFX_BORDER_LT;
    int i;

    gfx_fill(x, y, 14, 12, bg);
    gfx_frame(x, y, 14, 12, fg);

    if (kind == 0) {                    /* minimizar */
        gfx_hline(x + 4, y + 8, 6, fg);
    } else if (kind == 1) {             /* maximizar */
        gfx_frame(x + 4, y + 4, 6, 5, fg);
    } else {                            /* cerrar */
        for (i = 0; i < 5; i++) {
            gfx_pixel(x + 4 + i, y + 4 + i, fg);
            gfx_pixel(x + 8 - i, y + 4 + i, fg);
        }
    }
}

static void draw_window(wm_window_t *win)
{
    int x = win->x;
    int y = win->y;
    int w = win->w;
    int h = win->h;
    bool active = (win == focused);
    uint8_t bar = active ? GFX_TITLE_ON : GFX_TITLE_OFF;
    rect_t content;

    if (!win->visible)
        return;

    /* sombra */
    gfx_fill(x + 5, y + 5, w, h, GFX_SHADOW);

    /* marco */
    gfx_fill(x, y, w, h, GFX_PANEL);
    gfx_frame(x, y, w, h, active ? GFX_BORDER_LT : GFX_BORDER);

    /* barra de titulo */
    gfx_fill(x + 1, y + 1, w - 2, WM_TITLE_H, bar);
    gfx_text(x + 6, y + 5, win->title, active ? GFX_WHITE : GFX_MUTED, bar);

    title_button(x + w - 18, y + 3, 2, active);
    if (win->resizable || win->maximized)
        title_button(x + w - 34, y + 3, 1, active);
    title_button(x + w - 50, y + 3, 0, active);

    /* contenido */
    if (win->draw) {
        content.x = wm_content_x(win);
        content.y = wm_content_y(win);
        content.w = wm_content_w(win);
        content.h = wm_content_h(win);

        if (content.x < clip_rect.x) { content.w -= clip_rect.x - content.x; content.x = clip_rect.x; }
        if (content.y < clip_rect.y) { content.h -= clip_rect.y - content.y; content.y = clip_rect.y; }
        if (content.x + content.w > clip_rect.x + clip_rect.w)
            content.w = clip_rect.x + clip_rect.w - content.x;
        if (content.y + content.h > clip_rect.y + clip_rect.h)
            content.h = clip_rect.y + clip_rect.h - content.y;

        if (content.w > 0 && content.h > 0) {
            gfx_set_clip(content.x, content.y, content.w, content.h);
            win->draw(win);
            gfx_set_clip(clip_rect.x, clip_rect.y, clip_rect.w, clip_rect.h);
        }
    }

    /* esquina de redimensionado */
    if (win->resizable && !win->maximized) {
        int gx = x + w - 12;
        int gy = y + h - 12;
        int i;

        for (i = 0; i < 3; i++) {
            gfx_hline(gx + i * 3, gy + 9, 3, GFX_BORDER_LT);
            gfx_vline(gx + 9, gy + i * 3, 3, GFX_BORDER_LT);
        }
    }
}

static void draw_taskbar(void)
{
    int x = START_X + START_W + 6;
    int i;

    gfx_fill(0, WM_TASKBAR_Y, WM_SCREEN_W, WM_TASKBAR_H, GFX_PANEL);
    gfx_hline(0, WM_TASKBAR_Y, WM_SCREEN_W, GFX_BORDER);

    wm_button(START_X, START_Y, START_W, START_H, "Inicio", start_open);

    for (i = 0; i < stack_count; i++) {
        wm_window_t *win = stack[i];
        int width = wm_text_width(win->title) + 20;
        bool active = (win == focused) && win->visible;

        if (x + width > 660)
            break;

        gfx_fill(x, START_Y, width, START_H, active ? GFX_PANEL_ALT : GFX_BG);
        gfx_frame(x, START_Y, width, START_H,
                  active ? GFX_GREEN : (win->visible ? GFX_BORDER : GFX_TITLE_OFF));
        gfx_text(x + 10, START_Y + 7, win->title,
                 active ? GFX_GREEN : (win->visible ? GFX_TEXT : GFX_TERM_DIM),
                 active ? GFX_PANEL_ALT : GFX_BG);

        x += width + 4;
    }

    gfx_text(WM_SCREEN_W - 8 - 8 * 8, START_Y + 7, clock_buf, GFX_GREEN, GFX_PANEL);
}

static void draw_start_menu(void)
{
    int i;

    gfx_fill(MENU_X + 4, MENU_Y + 4, MENU_W, MENU_H, GFX_SHADOW);
    gfx_fill(MENU_X, MENU_Y, MENU_W, MENU_H, GFX_PANEL);
    gfx_frame(MENU_X, MENU_Y, MENU_W, MENU_H, GFX_BORDER_LT);

    gfx_text(MENU_X + 10, MENU_Y + 6, "Aplicaciones", GFX_MUTED, GFX_PANEL);
    gfx_hline(MENU_X + 1, MENU_Y + 18, MENU_W - 2, GFX_BORDER);

    for (i = 0; i < MENU_ITEMS; i++) {
        int y = MENU_Y + 22 + i * MENU_ITEM_H;

        gfx_text(MENU_X + 12, y + 8, start_items[i].name, GFX_TEXT, GFX_PANEL);
        gfx_text(MENU_X + MENU_W - 14, y + 8, ">", GFX_TERM_DIM, GFX_PANEL);
    }
}

static void paint(rect_t area)
{
    int i;

    clip_rect = area;
    gfx_set_clip(area.x, area.y, area.w, area.h);

    draw_desktop_background();
    apps_draw_desktop();

    for (i = 0; i < stack_count; i++)
        draw_window(stack[i]);

    draw_taskbar();

    if (start_open)
        draw_start_menu();

    gfx_flush(area.x, area.y, area.w, area.h);
    gfx_clip_off();
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.w = WM_SCREEN_W;
    clip_rect.h = WM_SCREEN_H;
}

/* crece el area danada hasta incluir toda ventana que la solape: asi al
 * repintar en orden se respeta la superposicion sin listas de oclusion */
static void grow_damage(void)
{
    int i, j, round, changed;

    /* Como mucho hace falta una pasada por ventana: el area solo crece y hay
     * pocas ventanas, pero el limite evita cualquier bucle accidental. */
    for (round = 0; round < WM_MAX_WINDOWS + 1; round++) {
        changed = 0;

        for (i = 0; i < damage_count; i++) {
            rect_t *d = &damage[i];

            for (j = 0; j < stack_count; j++) {
                wm_window_t *win = stack[j];
                int wx, wy, x2, y2, nx1, ny1, nx2, ny2;

                if (!win->visible)
                    continue;

                wx = win->x;
                wy = win->y;
                x2 = win->x + win->w + 6;
                y2 = win->y + win->h + 6;

                if (wx >= d->x + d->w || x2 <= d->x ||
                    wy >= d->y + d->h || y2 <= d->y)
                    continue;

                nx1 = wx < d->x ? wx : d->x;
                ny1 = wy < d->y ? wy : d->y;
                nx2 = x2 > d->x + d->w ? x2 : d->x + d->w;
                ny2 = y2 > d->y + d->h ? y2 : d->y + d->h;

                if (nx1 == d->x && ny1 == d->y &&
                    nx2 == d->x + d->w && ny2 == d->y + d->h)
                    continue;               /* ya cubre esa ventana */

                d->x = nx1;
                d->y = ny1;
                d->w = nx2 - nx1;
                d->h = ny2 - ny1;
                changed = 1;

                if (d->w * d->h > WM_SCREEN_W * WM_SCREEN_H / 2) {
                    damage_all = true;
                    return;
                }
            }
        }

        if (!changed)
            return;
    }
}

static void flush_damage(void)
{
    int i;

    if (!damage_all && damage_count > 0)
        grow_damage();

    if (damage_all) {
        rect_t all = { 0, 0, WM_SCREEN_W, WM_SCREEN_H };
        paint(all);
        damage_all = false;
        damage_count = 0;
        return;
    }

    for (i = 0; i < damage_count; i++)
        paint(damage[i]);

    damage_count = 0;
}

/* ------------------------------------------------------------------ */
/* Cursor                                                              */
/* ------------------------------------------------------------------ */

static void cursor_hide(void)
{
    if (!cursor_up)
        return;

    gfx_restore(cursor_x, cursor_y, 10, 16);
    cursor_up = false;
}

static void cursor_show(void)
{
    if (cursor_up)
        return;

    gfx_cursor_draw(mouse_x, mouse_y);
    cursor_x = mouse_x;
    cursor_y = mouse_y;
    cursor_up = true;
}

/* ------------------------------------------------------------------ */
/* Aplicaciones del menu                                               */
/* ------------------------------------------------------------------ */

static const start_item_t start_items[MENU_ITEMS] = {
    { "Terminal",     app_terminal_open },
    { "Archivos",     app_files_open    },
    { "Editor",       app_editor_open_none },
    { "Calculadora",  app_calc_open     },
    { "Sistema",      app_system_open   },
    { "Acerca de",    app_about_open    },
};

static int start_item_at(int x, int y)
{
    int i;

    for (i = 0; i < MENU_ITEMS; i++) {
        int iy = MENU_Y + 22 + i * MENU_ITEM_H;

        if (wm_hit(MENU_X, iy, MENU_W, MENU_ITEM_H, x, y))
            return i;
    }

    return -1;
}

static void open_app(int index)
{
    if (index >= 0 && index < MENU_ITEMS)
        start_items[index].open();
}

/* ------------------------------------------------------------------ */
/* Raton                                                               */
/* ------------------------------------------------------------------ */

static int task_button_at(int x)
{
    int bx = START_X + START_W + 6;
    int i;

    for (i = 0; i < stack_count; i++) {
        int width = wm_text_width(stack[i]->title) + 20;

        if (bx + width > 660)
            break;

        if (x >= bx && x < bx + width)
            return i;

        bx += width + 4;
    }

    return -1;
}

static void toggle_maximize(wm_window_t *win)
{
    if (win->maximized) {
        win->x = win->restore_x;
        win->y = win->restore_y;
        win->w = win->restore_w;
        win->h = win->restore_h;
        win->maximized = false;
    } else {
        win->restore_x = win->x;
        win->restore_y = win->y;
        win->restore_w = win->w;
        win->restore_h = win->h;

        win->x = 0;
        win->y = 0;
        win->w = WM_SCREEN_W;
        win->h = WM_TASKBAR_Y;
        win->maximized = true;
    }

    wm_invalidate();
}

static void minimize_window(wm_window_t *win)
{
    win->visible = false;
    focused = NULL;
    wm_invalidate();
}

static void window_press(wm_window_t *win, int x, int y, bool double_click)
{
    if (y < win->y + WM_TITLE_H) {
        if (wm_hit(win->x + win->w - 18, win->y + 3, 14, 12, x, y)) {
            wm_destroy(win);
            return;
        }

        if (wm_hit(win->x + win->w - 34, win->y + 3, 14, 12, x, y)) {
            toggle_maximize(win);
            return;
        }

        if (wm_hit(win->x + win->w - 50, win->y + 3, 14, 12, x, y)) {
            minimize_window(win);
            return;
        }

        drag_mode = 1;
        drag_win = win;
        drag_off_x = x - win->x;
        drag_off_y = y - win->y;
        return;
    }

    if (win->resizable && !win->maximized &&
        wm_hit(win->x + win->w - 14, win->y + win->h - 14, 14, 14, x, y)) {
        drag_mode = 2;
        drag_win = win;
        return;
    }

    if (win->click)
        win->click(win, x, y, double_click);
}

static void mouse_press(int x, int y)
{
    int i;
    bool double_click = false;
    int dx;
    int dy;


    dx = x > last_click_x ? x - last_click_x : last_click_x - x;
    dy = y > last_click_y ? y - last_click_y : last_click_y - y;

    if (timer_ticks() - last_click_tick <= 60 && dx <= 4 && dy <= 4)
        double_click = true;

    last_click_tick = timer_ticks();
    last_click_x = x;
    last_click_y = y;

    if (start_open) {
        int item = start_item_at(x, y);

        start_open = false;

        if (item >= 0) {
            wm_invalidate();
            open_app(item);
            return;
        }

        wm_invalidate();
    }

    if (y >= WM_TASKBAR_Y) {
        if (wm_hit(START_X, START_Y, START_W, START_H, x, y)) {
            start_open = !start_open;
            wm_invalidate();
            return;
        }

        i = task_button_at(x);
        if (i >= 0) {
            wm_window_t *win = stack[i];

            if (!win->visible || focused != win) {
                win->visible = true;
                wm_focus(win);
            } else {
                minimize_window(win);
            }
            wm_invalidate();
            return;
        }

        return;
    }

    for (i = stack_count - 1; i >= 0; i--) {
        wm_window_t *win = stack[i];

        if (!win->visible)
            continue;

        if (!wm_hit(win->x, win->y, win->w, win->h, x, y))
            continue;

        wm_focus(win);
        window_press(win, x, y, double_click);
        return;
    }

    apps_desktop_click(x, y);
}

static void mouse_move_drag(int x, int y)
{
    if (!drag_win)
        return;

    if (drag_mode == 1) {
        int nx = x - drag_off_x;
        int ny = y - drag_off_y;

        if (nx < -(drag_win->w - 80)) nx = -(drag_win->w - 80);
        if (nx > WM_SCREEN_W - 80) nx = WM_SCREEN_W - 80;
        if (ny < 0) ny = 0;
        if (ny > WM_TASKBAR_Y - WM_TITLE_H) ny = WM_TASKBAR_Y - WM_TITLE_H;

        drag_win->x = nx;
        drag_win->y = ny;
        drag_win->maximized = false;
        wm_invalidate();
        return;
    }

    if (drag_mode == 2) {
        int w = x - drag_win->x;
        int h = y - drag_win->y;

        if (w < drag_win->min_w) w = drag_win->min_w;
        if (h < drag_win->min_h) h = drag_win->min_h;
        if (drag_win->x + w > WM_SCREEN_W) w = WM_SCREEN_W - drag_win->x;
        if (drag_win->y + h > WM_TASKBAR_Y) h = WM_TASKBAR_Y - drag_win->y;

        drag_win->w = w;
        drag_win->h = h;
        wm_invalidate();
    }
}

static void handle_mouse(void)
{
    mouse_event_t event;
    int nx;
    int ny;
    bool moved;
    bool clicked;
    bool release;

    mouse_poll();

    if (!mouse_read(&event))
        return;

    nx = mouse_x + event.dx;
    ny = mouse_y + event.dy;

    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > WM_SCREEN_W - 8) nx = WM_SCREEN_W - 8;
    if (ny > WM_SCREEN_H - 12) ny = WM_SCREEN_H - 12;

    moved = (nx != mouse_x || ny != mouse_y);
    release = !event.left && mouse_left_before;
    mouse_left_before = event.left;
    clicked = mouse_take_click();

    if (moved) {
        cursor_hide();
        mouse_x = nx;
        mouse_y = ny;
        cursor_show();
    }

    if (clicked)
        mouse_press(mouse_x, mouse_y);

    if (drag_mode)
        mouse_move_drag(mouse_x, mouse_y);

    if (release)
        drag_mode = 0;
}

/* ------------------------------------------------------------------ */
/* Teclado                                                             */
/* ------------------------------------------------------------------ */

static void handle_keys(void)
{
    int key;

    while ((key = keyboard_poll()) >= 0) {
        if (start_open) {
            start_open = false;
            wm_invalidate();
        }

        if (focused && focused->key && focused->visible)
            focused->key(focused, key);
        else if (focused && focused->visible)
            terminal_key(key);
    }

    while ((key = keyboard_poll_special()) >= 0) {
        if (key == 27) {
            start_open = false;
            wm_invalidate();
        }

        if (key >= 1 && key <= 3) {
            if (key == 1) app_terminal_open();
            if (key == 2) app_files_open();
            if (key == 3) app_calc_open();
            wm_invalidate();
        }
    }
}

void gui_mouse_warp(int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > WM_SCREEN_W - 8) x = WM_SCREEN_W - 8;
    if (y > WM_SCREEN_H - 12) y = WM_SCREEN_H - 12;

    cursor_hide();
    mouse_x = x;
    mouse_y = y;
    cursor_show();
}

void gui_list_windows(void)
{
    int i;

    kprintf("ventanas: %d\n", stack_count);

    for (i = 0; i < stack_count; i++) {
        wm_window_t *win = stack[i];

        kprintf("  %d  %-18s %dx%d en %d,%d %s%s\n",
                win->id, win->title, win->w, win->h, win->x, win->y,
                win->visible ? "visible" : "minimizada",
                win == focused ? " con foco" : "");
    }
}

/* ------------------------------------------------------------------ */
/* Arranque del escritorio                                             */
/* ------------------------------------------------------------------ */

static void update_clock(void)
{
    rtc_time_t now;

    rtc_read(&now);
    ksnprintf(clock_buf, sizeof(clock_buf), "%02u:%02u:%02u",
              now.hour, now.minute, now.second);
}

void wm_start(void)
{
    int i;

    for (i = 0; i < WM_MAX_WINDOWS; i++) {
        pool_used[i] = false;
        stack[i] = NULL;
    }

    stack_count = 0;
    focused = NULL;
    damage_count = 0;
    damage_all = false;
    start_open = false;
    drag_mode = 0;
    drag_win = NULL;
    mouse_left_before = true;      /* evita un clic fantasma al entrar */
    cursor_up = false;
    wm_running = true;
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.w = WM_SCREEN_W;
    clip_rect.h = WM_SCREEN_H;

    update_clock();
    wm_invalidate();

    task_create("escritorio", wm_task, NULL);
}

static void wm_task(void *arg)
{
    uint32_t last_clock = 0;

    UNUSED(arg);

    for (;;) {
        handle_mouse();
        handle_keys();

        if (damage_all || damage_count) {
            cursor_hide();
            flush_damage();
            cursor_show();
        }

        if (timer_ticks() - last_clock >= 50) {
            last_clock = timer_ticks();
            update_clock();
            wm_damage(WM_SCREEN_W - 8 - 8 * 8 - 4, WM_TASKBAR_Y, 80, WM_TASKBAR_H);
        }

        cursor_show();
        hlt();
    }
}
