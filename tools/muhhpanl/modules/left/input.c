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

static void input_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void input_draw(Module *m, int x, int y, int w, int h, int focused) {
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

  int pad = MODULE_PADDING, font_h = drw->fonts->h;
  const char *label = "Input";
  int tw = drw_fontset_getwidth(drw, label);
  drw_setscheme(drw, scheme[0]);
  drw_text(drw, x + pad, y + (h - font_h) / 2, tw, font_h, 0, label, 0);
}

static void input_input(Module *m, const InputEvent *ev) { /* placeholder */ }

Module input_module = {
    .name = "input",
    .init = input_init,
    .draw = input_draw,
    .input = input_input,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
};

void __attribute__((constructor)) input_register(void) {
  register_module(&input_module);
}
