#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"

#define TEXT_RECENT_FILE "/home/swaminsane/.cache/textsmenu_recent"
#define TEXT_SCRIPT "/home/swaminsane/.local/bin/menu/textsmenu"

typedef struct {
  char title[128];
  char path[512];
  int valid;
  XftColor green_color; /* pre‑allocated colour for NCERT */
  XftColor blue_color;  /* pre‑allocated colour for Books */
} TextState;

/* ── allocate a colour from 0xRRGGBB ──────────────────── */
static void alloc_rgb_color(unsigned long rgb, XftColor *dst) {
  XRenderColor xrc = {.red = ((rgb >> 16) & 0xFF) * 0x101,
                      .green = ((rgb >> 8) & 0xFF) * 0x101,
                      .blue = (rgb & 0xFF) * 0x101,
                      .alpha = 0xFFFF};
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                     DefaultColormap(dpy, screen), &xrc, dst);
}

/* ── choose colour from path ──────────────────────────── */
static XftColor *text_color_for(TextState *s) {
  if (!s->valid)
    return &scheme[0][ColFg];
  const char *p = s->path;
  while (*p) {
    if (strncasecmp(p, "ncert", 5) == 0)
      return &s->green_color;
    if (strncasecmp(p, "books", 5) == 0)
      return &s->blue_color;
    p++;
  }
  return &scheme[0][ColFg]; /* default foreground */
}

/* ── read most recent entry from cache ────────────────── */
static void refresh_recent(TextState *s) {
  s->valid = 0;
  s->title[0] = '\0';
  s->path[0] = '\0';

  FILE *f = fopen(TEXT_RECENT_FILE, "r");
  if (!f)
    return;

  char line[512];
  if (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0]) {
      strncpy(s->path, line, sizeof(s->path) - 1);
      const char *base = strrchr(line, '/');
      base = base ? base + 1 : line;
      char clean[128];
      snprintf(clean, sizeof(clean), "%s", base);
      char *dot = strrchr(clean, '.');
      if (dot)
        *dot = '\0';
      strncpy(s->title, clean, sizeof(s->title) - 1);
      s->valid = 1;
    }
  }
  fclose(f);
}

/* ── callbacks ────────────────────────────────────────── */
static void texts_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  TextState *s = calloc(1, sizeof(TextState));
  m->priv = s;

  alloc_rgb_color(0xa3be8c, &s->green_color); /* COL_GREEN */
  alloc_rgb_color(0x81a1c1, &s->blue_color);  /* COL_BLUE */
  refresh_recent(s);
}

static void texts_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  TextState *s = (TextState *)m->priv;

  /* card background */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  /* thin border using the card theme's border colour */
  XSetForeground(dpy, drw->gc, module_card_theme.border);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  int pad = MODULE_PADDING;
  int inner_x = x + pad, inner_y = y + pad;
  int inner_w = w - 2 * pad, inner_h = h - 2 * pad;
  int font_h = drw->fonts->h;

  char display[160];
  snprintf(display, sizeof(display), "Reading - %s",
           s->valid ? s->title : "No book");

  /* build a tiny scheme that uses our custom foreground */
  Clr custom_scheme[3];
  custom_scheme[ColFg] = *text_color_for(s);
  custom_scheme[ColBg] = scheme[2][ColBg]; /* same as card background */
  custom_scheme[ColBorder] = scheme[0][ColBorder];
  drw_setscheme(drw, custom_scheme);
  drw_text(drw, inner_x, inner_y + (inner_h - font_h) / 2, inner_w, font_h, 0,
           display, 0);
  drw_setscheme(drw, scheme[0]); /* restore default */
}

static void texts_input(Module *m, const InputEvent *ev) {
  TextState *s = (TextState *)m->priv;
  if (ev->type != EV_PRESS)
    return;

  if (ev->button == Button1 && s->valid) {
    const char *ext = strrchr(s->path, '.');
    const char *reader = "zathura";
    if (ext && (strcasecmp(ext, ".epub") == 0 || strcasecmp(ext, ".mobi") == 0))
      reader = "mupdf";
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" &", reader, s->path);
    system(cmd);
    panel_hide(); /* close panel after opening book */
  } else if (ev->button == Button3) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s NCERT &", TEXT_SCRIPT);
    system(cmd);
    panel_hide(); /* close panel after launching menu */
  } else if (ev->button == Button2) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s Books &", TEXT_SCRIPT);
    system(cmd);
    panel_hide(); /* close panel after launching menu */
  }
}

static void texts_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 10) {
    last = now;
    TextState *s = (TextState *)m->priv;
    refresh_recent(s);
    panel_redraw();
  }
}

static void texts_destroy(Module *m) {
  TextState *s = (TextState *)m->priv;
  if (s) {
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &s->green_color);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &s->blue_color);
    free(s);
  }
}

static LayoutHints *texts_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 24, .pref_h = 24, .max_h = 24, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module textsmenu_module = {
    .name = M_MID_TEXTSMENU,
    .init = texts_init,
    .draw = texts_draw,
    .input = texts_input,
    .timer = texts_timer,
    .destroy = texts_destroy,
    .get_hints = texts_hints,
    .margin_top = 2,
    .margin_bottom = 2,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) textsmenu_register(void) {
  register_module(&textsmenu_module);
}
