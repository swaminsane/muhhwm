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

typedef struct {
  int radio_on; /* 1 = WiFi radio enabled, 0 = disabled */
} WifiState;

/* ── helper: run shell command and return first line ─────── */
static char *run_getline(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return NULL;
  char *buf = malloc(256);
  if (buf && fgets(buf, 256, f)) {
    pclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return buf;
  }
  free(buf);
  pclose(f);
  return NULL;
}

/* ── refresh WiFi radio state ────────────────────────────── */
static void wifi_refresh(WifiState *s) {
  char *radio = run_getline("nmcli radio wifi 2>/dev/null");
  s->radio_on = (radio && strcmp(radio, "enabled") == 0);
  free(radio);
}

/* ── toggle radio on / off ────────────────────────────────── */
static void wifi_toggle(void) {
  char *radio = run_getline("nmcli radio wifi 2>/dev/null");
  if (radio && strcmp(radio, "enabled") == 0)
    system("nmcli radio wifi off");
  else
    system("nmcli radio wifi on");
  free(radio);
}

/* ── module callbacks ────────────────────────────────────── */
static void wifi_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  WifiState *s = calloc(1, sizeof(WifiState));
  m->priv = s;
  wifi_refresh(s);
}

static void wifi_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  WifiState *s = (WifiState *)m->priv;

  /* background */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* border – green when on, grey when off */
  const char *border_color = s->radio_on ? COL_GREEN : COL_BRIGHT_BLACK;
  XColor bdc;
  XParseColor(dpy, DefaultColormap(dpy, screen), border_color, &bdc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bdc);
  XSetForeground(dpy, drw->gc, bdc.pixel);
  for (int i = 0; i < 4; i++)
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                   h - 1 - 2 * i);

  /* thin black separator */
  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, x + 4, y + 4, w - 8, h - 8);

  /* symbol – W when on, X when off */
  const char *symbol = s->radio_on ? "W" : "w";
  XftColor *fg = &scheme[0][ColFg];
  Fnt *font = drw->fonts;
  int tw = drw_fontset_getwidth(drw, symbol);
  int icon_x = x + (w - tw) / 2;
  int icon_y = y + (h - font->h) / 2 + font->xfont->ascent;

  XftDraw *xftdraw =
      XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen));
  XftDrawStringUtf8(xftdraw, fg, font->xfont, icon_x, icon_y,
                    (const XftChar8 *)symbol, strlen(symbol));
  XftDrawDestroy(xftdraw);
}

static void wifi_input(Module *m, const InputEvent *ev) {
  WifiState *s = (WifiState *)m->priv;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    wifi_toggle();
    wifi_refresh(s);
    panel_redraw();
    return;
  }

  if (ev->type == EV_PRESS && ev->button == Button3) {
    system("$HOME/.local/bin/menu/connectmenu --wifi &");
    return;
  }
}

static void wifi_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 5) {
    last = now;
    WifiState *s = (WifiState *)m->priv;
    wifi_refresh(s);
    panel_redraw();
  }
}

static void wifi_destroy(Module *m) { free(m->priv); }

static LayoutHints *wifi_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_w = 50,
      .pref_w = 50,
      .max_w = 50,
      .min_h = 50,
      .pref_h = 50,
      .max_h = 50,
      .expand_x = 0,
      .expand_y = 0,
  };
  return &hints;
}

Module wifi_module = {
    .name = "wifi",
    .init = wifi_init,
    .draw = wifi_draw,
    .input = wifi_input,
    .timer = wifi_timer,
    .destroy = wifi_destroy,
    .get_hints = wifi_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) wifi_register(void) {
  register_module(&wifi_module);
}
