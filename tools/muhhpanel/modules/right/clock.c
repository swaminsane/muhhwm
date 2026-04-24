#include "../common/container.h"
#include "../common/drw.h"
#include "../common/module.h"
#include "../common/panel_globals.h"
#include "../panel.h"
#include "../settings.h"
#include <string.h>
#include <time.h>

static time_t last_second = 0;

static void clock_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  last_second = 0;
}

static void clock_draw(Module *m, int x, int y, int w, int h, int focused) {
  /* card background (theme) */
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

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  /* time: HH:MM (24‑hour) */
  char time_str[6];
  strftime(time_str, sizeof(time_str), "%H:%M", tm);

  /* date: Fri, 24 Apr */
  char date_str[32];
  strftime(date_str, sizeof(date_str), "%a, %d %b", tm);

  int font_h = drw->fonts->h;

  /* measure text widths */
  int tw_time = drw_fontset_getwidth(drw, time_str);
  int tw_date = drw_fontset_getwidth(drw, date_str);

  /* vertical placement: time on top, date just below */
  int total_h = font_h * 2 + 6; /* two lines + small gap */
  int start_y = inner_y + (inner_h - total_h) / 2;
  if (start_y < inner_y)
    start_y = inner_y;

  drw_setscheme(drw, scheme[0]); /* normal text colour */
  drw_text(drw, inner_x + (inner_w - tw_time) / 2, start_y, tw_time, font_h, 0,
           time_str, 0);
  drw_text(drw, inner_x + (inner_w - tw_date) / 2, start_y + font_h + 6,
           tw_date, font_h, 0, date_str, 0);
}

static void clock_timer(Module *m) {
  time_t now = time(NULL);
  if (now != last_second) {
    last_second = now;
    panel_redraw();
  }
}

Module clock_module = {
    .name = "clock",
    .init = clock_init,
    .draw = clock_draw,
    .timer = clock_timer,
    .theme = (ContainerTheme *)&module_card_theme,
    .priv = NULL,
};

void __attribute__((constructor)) clock_register(void) {
  register_module(&clock_module);
}
