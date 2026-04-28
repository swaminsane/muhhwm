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

/* ── read battery percentage and charging status ── */
static void read_battery(int *pct, int *charging) {
  *pct = 100;
  *charging = 0;
  FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
  if (f) {
    fscanf(f, "%d", pct);
    fclose(f);
  }
  f = fopen("/sys/class/power_supply/BAT0/status", "r");
  if (f) {
    char s[16] = {0};
    fgets(s, sizeof(s), f);
    fclose(f);
    *charging = (strncmp(s, "Charging", 8) == 0);
  }
}

/* ── allocate a pixel colour from a hex string ─── */
static unsigned long xpixel(const char *hex) {
  XColor col;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &col);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &col);
  return col.pixel;
}

/* ── allocate an XftColor from a hex string ─────── */
static XftColor xft_pixel(const char *hex) {
  XColor xc;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &xc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
  XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
  XftColor col;
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                     DefaultColormap(dpy, screen), &xrc, &col);
  return col;
}

static void ts_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void ts_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;

  /* background */
  XSetForeground(dpy, drw->gc, 0x3B4252);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* bottom border */
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawLine(dpy, drw->drawable, drw->gc, x, y + h - 1, x + w - 1, y + h - 1);

  int font_h = drw->fonts->h;
  int pad = MODULE_PADDING;

  /* ── left: battery percentage ── */
  {
    int bat_pct, charging;
    read_battery(&bat_pct, &charging);

    char bstr[16];
    if (charging)
      snprintf(bstr, sizeof(bstr), "%d%% *", bat_pct);
    else
      snprintf(bstr, sizeof(bstr), "%d%%", bat_pct);

    /* choose colour */
    const char *bcol;
    if (charging)
      bcol = COL_GREEN;
    else if (bat_pct < 15)
      bcol = COL_RED;
    else
      bcol = COL_FG; /* normal foreground */

    XftColor fg = xft_pixel(bcol);
    int text_y = y + (h - font_h) / 2;

    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xft, &fg, drw->fonts->xfont, x + pad,
                      text_y + drw->fonts->xfont->ascent,
                      (const XftChar8 *)bstr, strlen(bstr));
    XftDrawDestroy(xft);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &fg);
  }

  /* ── right: chill / chilling button ── */
  {
    const char *chill_label = chill_mode ? "chilling" : "chill";
    int cw = drw_fontset_getwidth(drw, chill_label);
    int bx = x + w - pad - cw - 4;
    int by = y + (h - font_h) / 2;

    unsigned long btn_bg = xpixel(chill_mode ? COL_GREEN : COL_BLUE);
    XSetForeground(dpy, drw->gc, btn_bg);
    XFillRectangle(dpy, drw->drawable, drw->gc, bx - 4, y + 2, cw + 8, h - 4);

    XftColor white = xft_pixel("#ffffff");
    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xft, &white, drw->fonts->xfont, bx,
                      by + drw->fonts->xfont->ascent,
                      (const XftChar8 *)chill_label, strlen(chill_label));
    XftDrawDestroy(xft);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &white);
  }
}

/* ── input handler ──────────────────────────────── */
static void ts_input(Module *m, const InputEvent *ev) {

  /* chill button click */
  {
    int pad = MODULE_PADDING;
    const char *chill_label = chill_mode ? "chilling" : "chill";
    int cw = drw_fontset_getwidth(drw, chill_label);
    int bx = m->w - pad - cw - 4;
    if (ev->type == EV_PRESS && ev->button == Button1 && ev->x >= bx - 4 &&
        ev->x <= bx + cw + 4 && ev->y >= 0 && ev->y < m->h) {
      chill_mode = !chill_mode;
      if (chill_mode)
        profanity_hide();
      else if (panel_shown)
        profanity_show();
      panel_redraw();
      return;
    }
  }

  /* scroll up → close panel */
  if (ev->type == EV_SCROLL && ev->scroll_dy == 1)
    panel_hide();
}

static void ts_timer(Module *m) {
  /* redraw every 30s for battery update */
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 30) {
    last = now;
    panel_redraw();
  }
}

static LayoutHints *ts_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 20, .pref_h = 20, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module topstrip_module = {
    .name = M_TOPSTRIP,
    .init = ts_init,
    .draw = ts_draw,
    .input = ts_input,
    .timer = ts_timer,
    .get_hints = ts_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
    .destroy = NULL,
    .priv = NULL,
};

void __attribute__((constructor)) topstrip_register(void) {
  register_module(&topstrip_module);
}
