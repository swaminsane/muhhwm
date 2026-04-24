#include "timeline.h"
#include "../common/container.h"
#include "../common/drw.h"
#include "../common/module.h"
#include "../common/panel_globals.h"
#include "../settings.h"
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRIP_MINUTES 1440
#define DATA_FILE_MAX 512

/* workspace colours */
#define COL_IDLE 0x3D3D3D
#define COL_STUDY 0xC0392B
#define COL_CODE 0x2980B9
#define COL_FREE 0x27AE60
#define COL_STUDY_B 0xE74C3C
#define COL_CODE_B 0x3498DB
#define COL_FREE_B 0x2ECC71

/* timeline bar background */
#define BAR_BG 0x4C566A

static int hovered = 0;
static unsigned char ns_data[STRIP_MINUTES];
static char today_str[11] = "";
static int data_loaded = 0;

/* ── helpers ──────────────────────────────────────────── */
static void get_today(char *out, size_t sz) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(out, sz, "%Y-%m-%d", tm);
}

static int minute_of_day(void) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  return tm->tm_hour * 60 + tm->tm_min;
}

static void data_path(char *out, size_t sz) {
  const char *home = getenv("HOME");
  if (!home)
    home = "/tmp";
  snprintf(out, sz, "%s/.cache/muhhwm/daydata", home);
}

static void load_today_data(void) {
  char path[DATA_FILE_MAX];
  data_path(path, sizeof(path));
  get_today(today_str, sizeof(today_str));

  memset(ns_data, 0, sizeof(ns_data));
  FILE *f = fopen(path, "r");
  if (!f) {
    data_loaded = 1;
    return;
  }

  char line[64];
  while (fgets(line, sizeof(line), f)) {
    char date[11];
    int minute, ns;
    if (sscanf(line, "%10s %d %d", date, &minute, &ns) == 3) {
      if (strcmp(date, today_str) == 0 && minute >= 0 &&
          minute < STRIP_MINUTES && ns >= 0 && ns <= 3)
        ns_data[minute] = (unsigned char)ns;
    }
  }
  fclose(f);
  data_loaded = 1;
}

static void ensure_data_fresh(void) {
  char today[11];
  get_today(today, sizeof(today));
  if (strcmp(today, today_str) != 0 || !data_loaded)
    load_today_data();
}

static unsigned long ns_color(int ns, int bright) {
  switch (ns) {
  case 1:
    return bright ? COL_STUDY_B : COL_STUDY;
  case 2:
    return bright ? COL_CODE_B : COL_CODE;
  case 3:
    return bright ? COL_FREE_B : COL_FREE;
  default:
    return COL_IDLE;
  }
}

/* transparent text via Xft */
static void draw_text_transparent(int x, int y, const char *text,
                                  unsigned long rgb) {
  XftColor xft_col;
  XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                     DefaultColormap(dpy, DefaultScreen(dpy)),
                     &(XRenderColor){.red = ((rgb >> 16) & 0xFF) * 0x101,
                                     .green = ((rgb >> 8) & 0xFF) * 0x101,
                                     .blue = (rgb & 0xFF) * 0x101,
                                     .alpha = 0xFFFF},
                     &xft_col);
  XftDraw *xft_draw =
      XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, DefaultScreen(dpy)),
                    DefaultColormap(dpy, DefaultScreen(dpy)));
  XftDrawStringUtf8(xft_draw, &xft_col, drw->fonts->xfont, x, y,
                    (const XftChar8 *)text, strlen(text));
  XftDrawDestroy(xft_draw);
  XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
               DefaultColormap(dpy, DefaultScreen(dpy)), &xft_col);
}

/* ── module callbacks ──────────────────────────────────── */
static void tl_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
  load_today_data();
}

static void tl_draw(Module *m, int x, int y, int w, int h, int focused) {
  ensure_data_fresh();
  int cur_min = minute_of_day();

  /* 1. Solid background */
  XSetForeground(dpy, drw->gc, BAR_BG);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* 2. Minute‑by‑minute workspace colours */
  for (int px = 0; px < w; px++) {
    int m = (px * STRIP_MINUTES) / w;
    if (m >= STRIP_MINUTES)
      m = STRIP_MINUTES - 1;
    unsigned long col = ns_color(ns_data[m], 0);
    XSetForeground(dpy, drw->gc, col);
    XDrawLine(dpy, drw->drawable, drw->gc, x + px, y, x + px, y + h - 1);
  }

  /* 3. Bright indicator for the last 3 minutes */
  {
    int start_min = cur_min - 2;
    if (start_min < 0)
      start_min = 0;
    int start_px = (start_min * w) / STRIP_MINUTES;
    int end_px = ((cur_min + 1) * w) / STRIP_MINUTES;
    if (end_px > w)
      end_px = w;

    for (int px = start_px; px < end_px; px++) {
      int m = (px * STRIP_MINUTES) / w;
      unsigned long bright_col = ns_color(ns_data[m], 1);
      XSetForeground(dpy, drw->gc, bright_col);
      XDrawLine(dpy, drw->drawable, drw->gc, x + px, y, x + px, y + h - 1);
    }
  }

  /* 4. Hour separators */
  unsigned long sep_col = 0x888888;
  XSetForeground(dpy, drw->gc, sep_col);
  for (int hr = 0; hr <= 24; hr++) {
    int sep_x = x + (hr * w) / 24;
    if (sep_x < x + w)
      XDrawLine(dpy, drw->drawable, drw->gc, sep_x, y, sep_x, y + h - 1);
  }

  /* 5. Bottom border */
  XSetForeground(dpy, drw->gc, sep_col);
  XDrawLine(dpy, drw->drawable, drw->gc, x, y + h - 1, x + w - 1, y + h - 1);

  /* 6. Hour numbers on hover */
  if (hovered) {
    char buf[4];
    int hour_w = w / 24;
    unsigned long text_rgb = 0xDDDDDD;
    int text_y = y + h - 2;
    for (int hr = 0; hr < 24; hr++) {
      snprintf(buf, sizeof(buf), "%02d", hr);
      int tw = drw_fontset_getwidth(drw, buf);
      int tx = x + hr * hour_w + (hour_w - tw) / 2;
      draw_text_transparent(tx, text_y, buf, text_rgb);
    }
  }
}

/* classic motion callback – receives absolute coordinates */
static void tl_motion(Module *m, int x, int y) {
  /* sentinel: mouse left the panel completely */
  if (x == -1 && y == -1) {
    if (hovered) {
      hovered = 0;
      panel_redraw();
    }
    return;
  }

  int bar_top = m->y;
  int bar_bottom = m->y + m->h;
  int new_hover = (y >= bar_top && y < bar_bottom) ? 1 : 0;
  if (new_hover != hovered) {
    hovered = new_hover;
    panel_redraw();
  }
}

static void tl_timer(Module *m) {
  (void)m;
  ensure_data_fresh();
  panel_redraw();
}

Module timeline_module = {
    .name = "timeline",
    .init = tl_init,
    .draw = tl_draw,
    .click = NULL,
    .scroll = NULL,
    .timer = tl_timer,
    .motion = tl_motion,
    .destroy = NULL,
    .priv = NULL,
};

void timeline_register(void) { register_module(&timeline_module); }
