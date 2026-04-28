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

static const char *ns_names[] = {"study", "code", "free"};
static const char *ns_colors[] = {COL_RED, COL_BLUE, COL_GREEN};
#define N_NS 3

typedef struct {
  long long total_ms;
  long long ns_ms[N_NS];
  time_t last_check;

  int show_line;     /* 0 = screen time, 1 = namespace breakdown */
  int transitioning; /* 1 = blink in progress */
  struct timespec trans_start;
} DaystripState;

/* ── construct today's activity log path ─────────────── */
static void log_path(char *buf, size_t sz) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  const char *home = getenv("HOME");
  snprintf(buf, sz, "%s/.local/share/muhhwm/activity/%04d-%02d-%02d.log",
           home ? home : "/tmp", tm->tm_year + 1900, tm->tm_mon + 1,
           tm->tm_mday);
}

/* ── parse the activity file and fill state ───────────── */
static void load_daydata(DaystripState *s) {
  char path[512];
  log_path(path, sizeof(path));

  s->total_ms = 0;
  for (int i = 0; i < N_NS; i++)
    s->ns_ms[i] = 0;

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    long long ts, dur;
    char ns[32], cls[64], title[256];
    int tag;
    if (sscanf(line, "%lld %lld %31s %d %63s \"%255[^\"]\"", &ts, &dur, ns,
               &tag, cls, title) < 5)
      continue;
    if (dur <= 0)
      continue;

    s->total_ms += dur;
    for (int i = 0; i < N_NS; i++) {
      if (strcmp(ns, ns_names[i]) == 0) {
        s->ns_ms[i] += dur;
        break;
      }
    }
  }
  fclose(f);
}

/* ── format milliseconds to short string ───── */
static void fmt_time(long long ms, char *buf, size_t sz) {
  int secs = (int)(ms / 1000);
  int hrs = secs / 3600;
  int mins = (secs % 3600) / 60;
  if (hrs > 0)
    snprintf(buf, sz, "%dh%02dm", hrs, mins);
  else
    snprintf(buf, sz, "%dm", mins);
}

/* ── allocate a pixel colour from a #define hex string ── */
static unsigned long xpixel(const char *hex) {
  XColor col;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &col);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &col);
  return col.pixel;
}

/* ── blink helpers ────────────────────────────────── */
static void start_blink(DaystripState *s) {
  s->transitioning = 1;
  clock_gettime(CLOCK_MONOTONIC, &s->trans_start);
}

static void finish_blink(DaystripState *s) {
  s->transitioning = 0;
  s->show_line = !s->show_line; /* toggle between 0 and 1 */
}

/* ── module callbacks ──────────────────────────────── */
static void daystrip_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  DaystripState *s = calloc(1, sizeof(DaystripState));
  m->priv = s;
  load_daydata(s);
  s->last_check = time(NULL);
  s->show_line = 0;
  start_blink(s);
}

static void daystrip_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  DaystripState *s = (DaystripState *)m->priv;

  /* card background during blink */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  if (s->transitioning)
    return;

  int font_h = drw->fonts->h;
  int pad = MODULE_PADDING;

  if (s->show_line == 0) {
    /* ── state 0: screen time on accent background ── */
    unsigned long accent = xpixel(COL_ACCENT);
    XSetForeground(dpy, drw->gc, accent);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

    char str[64];
    char timestr[32];
    fmt_time(s->total_ms, timestr, sizeof(timestr));
    snprintf(str, sizeof(str), "screen time %s", timestr);

    /* draw white text (Xft, no background) */
    XftColor fg;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), COL_BG, &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &fg);
    }
    Fnt *font = drw->fonts;
    int tw = drw_fontset_getwidth(drw, str);
    int text_x = x + (w - tw) / 2;
    int text_y = y + (h - font_h) / 2 + font->xfont->ascent;

    XftDraw *xftdraw =
        XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                      DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xftdraw, &fg, font->xfont, text_x, text_y,
                      (const XftChar8 *)str, strlen(str));
    XftDrawDestroy(xftdraw);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &fg);
  } else {
    /* ── state 1: coloured bars with evenly spaced labels ── */
    int total_bar_w = w - 2 * pad - 16;
    if (total_bar_w < 10)
      total_bar_w = 10;

    int bar_y = y + 4;
    int bar_h = h - 8;

    /* pre‑allocate black text colour */
    XftColor black;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), "#000000", &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &black);
    }

    /* first draw the three proportional coloured bars */
    int cur_x = x + pad + 8;
    for (int i = 0; i < N_NS; i++) {
      float frac = s->total_ms > 0 ? (float)s->ns_ms[i] / s->total_ms : 0.0f;
      int bar_w = (int)(frac * total_bar_w);
      if (bar_w < 2)
        bar_w = 2;

      unsigned long bar_col = xpixel(ns_colors[i]);
      XSetForeground(dpy, drw->gc, bar_col);
      XFillRectangle(dpy, drw->drawable, drw->gc, cur_x, bar_y, bar_w, bar_h);
      cur_x += bar_w;
    }

    /* now overlay the three labels, each centred in its equal 1/3 of the bar */
    int section_w = total_bar_w / N_NS; /* equal width for each label */
    XftDraw *xftdraw =
        XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                      DefaultColormap(dpy, screen));
    for (int i = 0; i < N_NS; i++) {
      char label[64];
      char tbuf[32];
      fmt_time(s->ns_ms[i], tbuf, sizeof(tbuf));
      snprintf(label, sizeof(label), "%s %s", ns_names[i], tbuf);

      int lw = drw_fontset_getwidth(drw, label);
      int section_start_x = x + pad + 8 + i * section_w;
      int text_x = section_start_x + (section_w - lw) / 2;
      if (text_x < section_start_x)
        text_x = section_start_x;

      int text_y = bar_y + (bar_h - font_h) / 2 + drw->fonts->xfont->ascent;
      XftDrawStringUtf8(xftdraw, &black, drw->fonts->xfont, text_x, text_y,
                        (const XftChar8 *)label, strlen(label));
    }
    XftDrawDestroy(xftdraw);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &black);
  }
}

static void daystrip_timer(Module *m) {
  DaystripState *s = (DaystripState *)m->priv;

  /* update data every 30s */
  time_t now = time(NULL);
  if (now - s->last_check >= 30) {
    load_daydata(s);
    s->last_check = now;
    panel_redraw();
  }

  /* handle blink transition */
  if (s->transitioning) {
    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);
    long elapsed_ms = (now_ts.tv_sec - s->trans_start.tv_sec) * 1000 +
                      (now_ts.tv_nsec - s->trans_start.tv_nsec) / 1000000;
    if (elapsed_ms >= DAYSTRIP_BLINK_MS) {
      finish_blink(s);
      panel_redraw();
    }
  }

  /* periodic toggle every DAYSTRIP_INTERVAL seconds */
  static time_t last_toggle = 0;
  if (!s->transitioning && now - last_toggle >= DAYSTRIP_INTERVAL) {
    last_toggle = now;
    start_blink(s);
    panel_redraw();
  }
}

static void daystrip_destroy(Module *m) { free(m->priv); }

static LayoutHints *daystrip_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 24,
      .pref_h = 24,
      .max_h = 24,
      .expand_y = 0,
      .expand_x = 1,
  };
  return &hints;
}

Module daystrip_module = {
    .name = "daystrip",
    .init = daystrip_init,
    .draw = daystrip_draw,
    .timer = daystrip_timer,
    .destroy = daystrip_destroy,
    .get_hints = daystrip_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) daystrip_register(void) {
  register_module(&daystrip_module);
}
