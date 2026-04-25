#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"

static time_t last_minute = 0;

static void clock_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  last_minute = 0;
}

static void clock_draw(Module *m, int x, int y, int w, int h, int focused) {
  /* card background */
  if (m->theme) {
    XSetForeground(dpy, drw->gc, m->theme->bg);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
    if (m->theme->border_w > 0) {
      XSetForeground(dpy, drw->gc, m->theme->border);
      for (int i = 0; i < m->theme->border_w; i++)
        XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                       h - 1 - 2 * i);
    }
  }

  int pad = MODULE_PADDING + 4;
  int inner_x = x + pad, inner_y = y + pad;
  int inner_w = w - 2 * pad, inner_h = h - 2 * pad;
  if (inner_h < 1)
    inner_h = 1;

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  char time_str[6];
  strftime(time_str, sizeof(time_str), "%H:%M", tm);

  char date_str[32];
  strftime(date_str, sizeof(date_str), "%a, %d %b", tm);

  int font_h = drw->fonts->h;
  int tw_time = drw_fontset_getwidth(drw, time_str);
  int tw_date = drw_fontset_getwidth(drw, date_str);

  int total_h = font_h * 2 + 12;
  int start_y = inner_y + (inner_h - total_h) / 2;
  if (start_y < inner_y)
    start_y = inner_y;

  drw_setscheme(drw, scheme[0]);
  drw_text(drw, inner_x + (inner_w - tw_time) / 2, start_y, tw_time, font_h, 0,
           time_str, 0);
  drw_text(drw, inner_x + (inner_w - tw_date) / 2, start_y + font_h + 12,
           tw_date, font_h, 0, date_str, 0);
}

static void clock_input(Module *m, const InputEvent *ev) { /* no interaction */
}

static void clock_timer(Module *m) {
  time_t now = time(NULL);
  if (now != last_minute) {
    last_minute = now;
    panel_redraw();
  }
}

static LayoutHints *clock_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 80, .pref_h = 100, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module clock_module = {
    .name = "clock",
    .init = clock_init,
    .draw = clock_draw,
    .input = clock_input,
    .timer = clock_timer,
    .get_hints = clock_hints,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
};

void __attribute__((constructor)) clock_register(void) {
  register_module(&clock_module);
}
