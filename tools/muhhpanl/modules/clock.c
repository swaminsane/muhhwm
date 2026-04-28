#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
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

static const char *time_fonts[] = {
    "monospace:size=24:weight=bold:antialias=true:autohint=false"};

typedef struct {
  Fnt *time_font;
  int last_minute;
} ClockState;

static void format_date(char *buf, size_t sz) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  strftime(buf, sz, "%a, %d %b %Y", tm);
}

static void format_time(char *buf, size_t sz) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  strftime(buf, sz, "%H:%M:%S", tm);
}

static void full_datetime(char *buf, size_t sz) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  strftime(buf, sz, "%A, %d %B %Y, %H:%M:%S", tm);
}

static void clock_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  ClockState *s = calloc(1, sizeof(ClockState));
  m->priv = s;

  Fnt *old_fonts = drw->fonts;
  s->time_font = drw_fontset_create(drw, time_fonts, 1);
  drw->fonts = old_fonts;
  if (!s->time_font)
    s->time_font = drw->fonts;

  s->last_minute = -1;
  panel_redraw();
}

static void clock_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  ClockState *s = (ClockState *)m->priv;

  /* card background + border */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  int pad = MODULE_PADDING;
  int time_line_h = 48, date_line_h = 24, gap = 2;
  int block_h = time_line_h + gap + date_line_h;
  int block_y = y + (h - block_h) / 2; /* vertically centered */

  char time_str[16], date_str[32];
  format_time(time_str, sizeof(time_str));
  format_date(date_str, sizeof(date_str));

  /* ── time line ── */
  {
    int ty = block_y, th = time_line_h;
    Fnt *prev_font = drw->fonts;
    drw->fonts = s->time_font;
    int tw = drw_fontset_getwidth(drw, time_str);
    int fh = s->time_font->h;
    int tx = x + (w - tw) / 2; /* center horizontally */

    /* custom scheme: fg = accent, bg = card background */
    Clr time_scheme[3];
    time_scheme[ColFg] = scheme[1][ColBg]; /* COL_ACCENT */
    time_scheme[ColBg] = scheme[2][ColBg];
    time_scheme[ColBorder] = scheme[0][ColBorder];
    drw_setscheme(drw, time_scheme);
    drw_text(drw, tx, ty + (th - fh) / 2, tw, fh, 0, time_str, 0);
    drw->fonts = prev_font;
  }

  /* ── date line ── */
  {
    int dy = block_y + time_line_h + gap, dh = date_line_h;
    int tw = drw_fontset_getwidth(drw, date_str);
    int fh = drw->fonts->h;
    int tx = x + (w - tw) / 2; /* center horizontally */
    drw_setscheme(drw, scheme[0]);
    drw_text(drw, tx, dy + (dh - fh) / 2, tw, fh, 0, date_str, 0);
  }
}

static void clock_input(Module *m, const InputEvent *ev) {
  (void)m;
  if (ev->type != EV_PRESS)
    return;
  if (ev->button == Button1) {
    if (fork() == 0) {
      setsid();
      execlp("st", "st", "-e", "sh", "-c",
             "cal; echo; echo 'press any key…'; read -n1", NULL);
      _exit(1);
    }
  } else if (ev->button == Button2) {
    char buf[128];
    full_datetime(buf, sizeof(buf));
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "notify-send 'Clock' '%s'", buf);
    system(cmd);
  }
}

static void clock_timer(Module *m) {
  ClockState *s = (ClockState *)m->priv;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  int minute = tm->tm_hour * 60 + tm->tm_min;
  if (minute != s->last_minute) {
    s->last_minute = minute;
    panel_redraw();
  }
}

static void clock_destroy(Module *m) {
  ClockState *s = (ClockState *)m->priv;
  if (s) {
    if (s->time_font && s->time_font != drw->fonts)
      drw_fontset_free(s->time_font);
    free(s);
  }
}

static LayoutHints *clock_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 112,
      .pref_h = 112,
      .max_h = 112,
      .expand_y = 0,
      .expand_x = 1,
  };
  return &hints;
}

Module clock_module = {
    .name = "clock",
    .init = clock_init,
    .draw = clock_draw,
    .input = clock_input,
    .timer = clock_timer,
    .destroy = clock_destroy,
    .get_hints = clock_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) clock_register(void) {
  register_module(&clock_module);
}
