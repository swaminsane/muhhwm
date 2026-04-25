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

static void ts_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  last_minute = 0;
}

static void ts_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;

  /* Full‑width, full‑height background – makes the bar clearly visible */
  XSetForeground(dpy, drw->gc, 0x3B4252);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* Thin bottom border for separation */
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawLine(dpy, drw->drawable, drw->gc, x, y + h - 1, x + w - 1, y + h - 1);

  int font_h = drw->fonts->h;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  /* Clock on the left, perfectly centered vertically */
  char time_str[6];
  strftime(time_str, sizeof(time_str), "%H:%M", tm);
  int tw = drw_fontset_getwidth(drw, time_str);
  int text_y = y + (h - font_h) / 2;
  drw_setscheme(drw, scheme[0]);
  drw_text(drw, x + 10, text_y, tw, font_h, 0, time_str, 0);
}

/* Scroll handler – no coordinate check!  Any scroll that reaches us
 * means the pointer is over the top strip, so close the panel.       */
static void ts_input(Module *m, const InputEvent *ev) {
  if (ev->type == EV_SCROLL && ev->scroll_dy == 1) /* scroll up → close */
    panel_hide();
}

static void ts_timer(Module *m) {
  time_t now = time(NULL);
  if (now != last_minute) {
    last_minute = now;
    panel_redraw();
  }
}

/* Force the module to never shrink below 20 px, and fill the row */
static LayoutHints *ts_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 20, .pref_h = 20, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module topstrip_module = {
    .name = M_TOPSTRIP,
    .init = ts_init,
    .draw = ts_draw,
    .input = ts_input,
    .timer = ts_timer,
    .get_hints = ts_hints,
    .margin_top = 0, /* absolutely no margins – fill the row */
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
    .destroy = NULL,
    .priv = NULL,
};

void __attribute__((constructor)) topstrip_register(void) {
  register_module(&topstrip_module);
}
