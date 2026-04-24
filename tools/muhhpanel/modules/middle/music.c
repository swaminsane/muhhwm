#include "../../common/container.h"
#include "../../common/drw.h"
#include "../../common/module.h"
#include "../../common/panel_globals.h"
#include "../../panel.h"
#include "../../settings.h"
#include <X11/Xlib.h>

static void music_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}
static void music_draw(Module *m, int x, int y, int w, int h, int focused) {
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
  const char *label = "Music";
  int tw = drw_fontset_getwidth(drw, label);
  drw_setscheme(drw, scheme[0]);
  drw_text(drw, x + pad, y + (h - font_h) / 2, tw, font_h, 0, label, 0);
}
Module music_module = {
    .name = "music",
    .init = music_init,
    .draw = music_draw,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
};
void __attribute__((constructor)) music_register(void) {
  register_module(&music_module);
}
