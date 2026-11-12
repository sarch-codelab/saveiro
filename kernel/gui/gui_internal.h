#ifndef SAVEIRO_GUI_INTERNAL_H
#define SAVEIRO_GUI_INTERNAL_H

#include <types.h>
#include <gui.h>

#define WM_SCREEN_W    800
#define WM_SCREEN_H    600

#define WM_MAX_WINDOWS 8
#define WM_TITLE_H     18

#define WM_TASKBAR_H   30
#define WM_TASKBAR_Y   (WM_SCREEN_H - WM_TASKBAR_H)

typedef struct wm_window wm_window_t;

struct wm_window {
    int  id;
    char title[24];

    int  x, y, w, h;
    int  min_w, min_h;

    bool visible;          /* no esta minimizada */
    bool maximized;
    bool resizable;

    int  restore_x, restore_y, restore_w, restore_h;

    void *data;

    void (*draw)(wm_window_t *win);
    void (*click)(wm_window_t *win, int x, int y, bool double_click);
    void (*key)(wm_window_t *win, int key);
    void (*close)(wm_window_t *win);
};

/* ---- gestor de ventanas (wm.c) ---- */

void wm_start(void);
wm_window_t *wm_create(const char *title, int x, int y, int w, int h);
void wm_destroy(wm_window_t *win);
void wm_focus(wm_window_t *win);
void wm_set_title(wm_window_t *win, const char *title);
wm_window_t *wm_focused(void);

void wm_invalidate(void);
void wm_damage(int x, int y, int w, int h);
void wm_window_damage(wm_window_t *win);
void wm_content_damage(wm_window_t *win, int x, int y, int w, int h);
void wm_close_damage(void);

int  wm_content_x(wm_window_t *win);
int  wm_content_y(wm_window_t *win);
int  wm_content_w(wm_window_t *win);
int  wm_content_h(wm_window_t *win);

/* rectangulo que se esta repintando (para no dibujar de mas) */
void wm_clip_rect(int *x, int *y, int *w, int *h);

/* utilidades de dibujo para las aplicaciones */
bool wm_hit(int x, int y, int w, int h, int px, int py);
void wm_button(int x, int y, int w, int h, const char *label, bool active);
void wm_label(int x, int y, const char *text, uint8_t fg, uint8_t bg);
void wm_bar(int x, int y, int w, int h, uint32_t used, uint32_t total, uint8_t color);
int  wm_text_width(const char *text);

int  wm_window_count(void);
wm_window_t *wm_window_at(int index);

/* ---- aplicaciones (apps.c) ---- */

void apps_draw_desktop(void);
bool apps_desktop_click(int x, int y);

void app_terminal_open(void);
void app_files_open(void);
void app_editor_open(const char *name);
void app_editor_open_none(void);
void app_calc_open(void);
void app_system_open(void);
void app_about_open(void);

/* ---- terminal (gui.c) ---- */

void terminal_reset(void);
void terminal_attach(void);
void terminal_draw(wm_window_t *win);
void terminal_key(int key);
void terminal_push(char c);

#endif
