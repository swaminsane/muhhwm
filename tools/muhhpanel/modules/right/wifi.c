#include "../../common/container.h"
#include "../../common/drw.h"
#include "../../common/module.h"
#include "../../common/panel_globals.h"
#include "../../panel.h"
#include "../../settings.h"
#include <X11/Xlib.h>

static void wifi_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
}

static void wifi_draw(Module *m, int x, int y, int w, int h, int focused) {
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

  int pad = MODULE_PADDING;
  int inner_x = x + pad, inner_y = y + pad;
  int inner_w = w - 2 * pad, inner_h = h - 2 * pad;

  const char *label = "WiFi";
  int tw = drw_fontset_getwidth(drw, label);
  int font_h = drw->fonts->h;
  drw_setscheme(drw, scheme[0]);
  drw_text(drw, inner_x + (inner_w - tw) / 2, inner_y + (inner_h - font_h) / 2,
           tw, font_h, 0, label, 0);
}

Module wifi_module = {
    .name = "wifi",
    .init = wifi_init,
    .draw = wifi_draw,
    .theme = (ContainerTheme *)&module_card_theme,
    .priv = NULL,
};

void __attribute__((constructor)) wifi_register(void) {
  register_module(&wifi_module);
}
