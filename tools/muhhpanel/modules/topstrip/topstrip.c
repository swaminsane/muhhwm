#include "../common/container.h"
#include "../common/drw.h"
#include "../common/input.h"
#include "../common/module.h"
#include "../common/panel_globals.h"
#include "../settings.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>

#define STRIP_BG 0x2e3440
#define ISLAND_BG 0x88c0d0
#define TEXT_COLOR 0xd8dee9

static void ts_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void ts_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  XSetForeground(dpy, drw->gc, STRIP_BG);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  const char *island_text = "island";
  int tw = drw_fontset_getwidth(drw, island_text);
  int island_w = tw + 20;
  int island_h = h - 6;
  int island_x = x + (w - island_w) / 2;
  int island_y = y + 3;

  XSetForeground(dpy, drw->gc, ISLAND_BG);
  XFillRectangle(dpy, drw->drawable, drw->gc, island_x, island_y, island_w,
                 island_h);
  XSetForeground(dpy, drw->gc, TEXT_COLOR);
  drw_text(drw, island_x + 10, island_y + (island_h - drw->fonts->h) / 2, tw,
           drw->fonts->h, 0, island_text, 0);

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char time_str[6];
  strftime(time_str, sizeof(time_str), "%H:%M", tm);
  int tw_clock = drw_fontset_getwidth(drw, time_str);
  int clock_y = y + (h - drw->fonts->h) / 2;
  XSetForeground(dpy, drw->gc, TEXT_COLOR);
  drw_text(drw, x + 10, clock_y, tw_clock, drw->fonts->h, 0, time_str, 0);
}

static void ts_scroll(Module *m, int x, int y, int dir) {
  if (dir == 1) /* scroll up */
    panel_hide();
}

Module topstrip_module = {
    .name = "topstrip",
    .init = ts_init,
    .draw = ts_draw,
    .scroll = ts_scroll,
    .timer = NULL,
    .destroy = NULL,
    .priv = NULL,
};

void topstrip_register(void) { register_module(&topstrip_module); }
