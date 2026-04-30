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

/* ── run `muhhtime rawtoday` and parse its output ───────── */
static void load_muhhtime(DaystripState *s) {
  s->total_ms = 0;
  for (int i = 0; i < N_NS; i++)
    s->ns_ms[i] = 0;

  FILE *f = popen("muhhtime rawtoday 2>/dev/null", "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    /* handle "total active <minutes>" */
    if (strncmp(line, "total active ", 13) == 0) {
      long long min_val = 0;
      if (sscanf(line + 13, "%lld", &min_val) == 1)
        s->total_ms = min_val * 60000;
      continue;
    }

    /* handle "study <min>", "code <min>", "free <min>" */
    char key[64];
    long long val_min;
    if (sscanf(line, "%63s %lld", key, &val_min) != 2)
      continue;

    if (strcmp(key, "study") == 0) {
      s->ns_ms[0] = val_min * 60000;
    } else if (strcmp(key, "code") == 0) {
      s->ns_ms[1] = val_min * 60000;
    } else if (strcmp(key, "free") == 0) {
      s->ns_ms[2] = val_min * 60000;
    }
    /* hour lines are ignored */
  }
  pclose(f);
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
  s->show_line = !s->show_line;
}

/* ── module callbacks ──────────────────────────────── */
static void daystrip_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  DaystripState *s = calloc(1, sizeof(DaystripState));
  m->priv = s;
  load_muhhtime(s);
  s->last_check = time(NULL);
  s->show_line = 0;
  start_blink(s);
}

static void daystrip_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  DaystripState *s = (DaystripState *)m->priv;

  /* During blink, just show card background */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  if (s->transitioning) {
    /* still draw the border */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);
    return;
  }

  int font_h = drw->fonts->h;
  int pad = MODULE_PADDING;

  if (s->show_line == 0) {
    /* ── state 0: screen time on accent background ── */
    unsigned long accent = xpixel(COL_ACCENT);
    XSetForeground(dpy, drw->gc, accent);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

    /* thin border */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

    char str[64];
    char timestr[32];
    fmt_time(s->total_ms, timestr, sizeof(timestr));
    snprintf(str, sizeof(str), "screen time %s", timestr);

    /* draw black text */
    XftColor black;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), "#000000", &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &black);
    }
    int tw = drw_fontset_getwidth(drw, str);
    XftDraw *xftdraw =
        XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                      DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xftdraw, &black, drw->fonts->xfont, x + (w - tw) / 2,
                      y + (h - font_h) / 2 + drw->fonts->xfont->ascent,
                      (const XftChar8 *)str, strlen(str));
    XftDrawDestroy(xftdraw);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &black);
  } else {
    /* ── state 1: namespace breakdown on card background ── */
    /* thin border */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

    /* three proportional coloured bars */
    int total_bar_w = w - 2 * pad;
    int cur_x = x + pad;
    int bar_y = y + 2;
    int bar_h = h - 4;

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

    /* overlay labels centred in thirds */
    int section_w = total_bar_w / N_NS;
    XftColor black;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), "#000000", &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &black);
    }
    XftDraw *xftdraw =
        XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                      DefaultColormap(dpy, screen));
    for (int i = 0; i < N_NS; i++) {
      char label[64];
      char tbuf[32];
      fmt_time(s->ns_ms[i], tbuf, sizeof(tbuf));
      snprintf(label, sizeof(label), "%s %s", ns_names[i], tbuf);
      int lw = drw_fontset_getwidth(drw, label);
      int section_start_x = x + pad + i * section_w;
      XftDrawStringUtf8(xftdraw, &black, drw->fonts->xfont,
                        section_start_x + (section_w - lw) / 2,
                        bar_y + (bar_h - font_h) / 2 +
                            drw->fonts->xfont->ascent,
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
    load_muhhtime(s);
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
