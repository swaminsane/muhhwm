#include "../common/container.h"
#include "../common/module.h"
#include "../common/panel_globals.h"
#include "../panel.h"
#include "../settings.h"
#include <string.h>
#include <time.h>

static time_t last_second = 0;

static void clock_init(Module *m, int x, int y, int w, int h) {
  last_second = 0;
}

static void clock_draw(Module *m, int x, int y, int w, int h, int focused) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char time_str[16], date_str[16], day_str[16];
  strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
  strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm);
  strftime(day_str, sizeof(day_str), "%A", tm);

  int font_h = drw->fonts->h;
  int tw_time = drw_fontset_getwidth(drw, time_str);
  int tw_date = drw_fontset_getwidth(drw, date_str);
  int tw_day = drw_fontset_getwidth(drw, day_str);

  drw_setscheme(drw, scheme[0]);
  drw_rect(drw, x, y, w, h, 1, 1);
  drw_text(drw, x + (w - tw_time) / 2, y + h / 2 - font_h - 5, tw_time, font_h,
           0, time_str, 0);
  drw_text(drw, x + (w - tw_date) / 2, y + h / 2, tw_date, font_h, 0, date_str,
           0);
  drw_text(drw, x + (w - tw_day) / 2, y + h / 2 + font_h + 5, tw_day, font_h, 0,
           day_str, 0);
}

static void clock_timer(Module *m) {
  time_t now = time(NULL);
  if (now != last_second) {
    last_second = now;
    panel_redraw();
  }
}

void __attribute__((constructor)) clock_register(void) {
  static Module clock_mod = {.name = "clock",
                             .init = clock_init,
                             .draw = clock_draw,
                             .timer = clock_timer};
  register_module(&clock_mod);
}
