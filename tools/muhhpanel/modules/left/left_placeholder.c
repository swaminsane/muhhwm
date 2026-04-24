#include "../../common/container.h"
#include "../../common/drw.h"
#include "../../common/module.h"
#include "../../common/panel_globals.h"
#include "../../panel.h"
#include "../../settings.h"
#include <X11/Xlib.h>

static void lp_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void lp_draw(Module *m, int x, int y, int w, int h, int focused) {
  XSetForeground(dpy, drw->gc, 0xFF0000); /* bright red */
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  XSetForeground(dpy, drw->gc, 0xFFFF00); /* yellow border */
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  XSetForeground(dpy, drw->gc, 0xFFFFFF); /* white text */
  const char *label = "LEFT";
  int tw = drw_fontset_getwidth(drw, label);
  int font_h = drw->fonts->h;
  drw_text(drw, x + 10, y + (h - font_h) / 2, tw, font_h, 0, label, 0);
}

Module left_placeholder_module = {
    .name = "left_placeholder",
    .init = lp_init,
    .draw = lp_draw,
    .priv = NULL,
};

void __attribute__((constructor)) left_placeholder_register(void) {
  register_module(&left_placeholder_module);
}
