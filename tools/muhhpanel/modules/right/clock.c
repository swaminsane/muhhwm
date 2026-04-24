#include "../common/container.h"
#include "../common/drw.h"
#include "../common/module.h"
#include "../common/panel_globals.h"
#include "../panel.h"
#include "../settings.h"
#include <string.h>
#include <time.h>

static time_t last_minute = 0;

static void clock_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  last_minute = 0;
}

static LayoutHints *clock_get_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 80,   /* never shrink below this */
      .pref_h = 100, /* try to get this height */
      .expand_x = 1,
      .expand_y = 0 /* don't stretch beyond preferred */
  };
  return &hints;
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

  int pad = MODULE_PADDING + 4; /* extra padding inside the card */
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

  /* centre the two lines vertically */
  int total_h = font_h * 2 + 12;
  int start_y = inner_y + (inner_h - total_h) / 2;
  if (start_y < inner_y)
    start_y = inner_y;

  drw_setscheme(drw, scheme[0]); /* normal text colour */
  drw_text(drw, inner_x + (inner_w - tw_time) / 2, start_y, tw_time, font_h, 0,
           time_str, 0);
  drw_text(drw, inner_x + (inner_w - tw_date) / 2, start_y + font_h + 12,
           tw_date, font_h, 0, date_str, 0);
}

static void clock_timer(Module *m) {
  time_t now = time(NULL);
  if (now != last_minute) {
    last_minute = now;
    panel_redraw();
  }
}

Module clock_module = {
    .name = "clock",
    .init = clock_init,
    .draw = clock_draw,
    .get_hints = clock_get_hints,
    .timer = clock_timer,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
    .priv = NULL,
};

void __attribute__((constructor)) clock_register(void) {
  register_module(&clock_module);
}
