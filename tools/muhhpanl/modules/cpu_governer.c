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

/* ── governor (background) ──────────────────────────── */
static char **gov_list = NULL;
static int gov_count = 0;
static int cur_gov = 0;

/* ── EPP (border) ───────────────────────────────────── */
static const char *epp_values[] = {
    "power", "balance_power", "balance_performance", "default", "performance"};
static const char *epp_colors[] = {COL_GREEN, COL_CYAN, COL_BLUE, COL_YELLOW,
                                   COL_RED};
static const int epp_count = 5;
static int cur_epp = 0;

/* ── governor colours & letters ─────────────────────── */
static const char *gov_colors[] = {COL_RED, COL_GREEN};
static const char *gov_chars[] = {"Π", "Σ"};

/* ── load available governors from sysfs ────────────── */
static void load_available_governors(void) {
  FILE *f = fopen(
      "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors", "r");
  if (!f)
    return;
  char line[256];
  if (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    char *tok = strtok(line, " ");
    while (tok) {
      gov_list = realloc(gov_list, (gov_count + 1) * sizeof(char *));
      gov_list[gov_count++] = strdup(tok);
      tok = strtok(NULL, " ");
    }
  }
  fclose(f);
}

/* ── read current governor ──────────────────────────── */
static int read_current_gov(void) {
  FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
  if (!f)
    return 0;
  char line[32];
  if (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    for (int i = 0; i < gov_count; i++)
      if (!strcmp(line, gov_list[i])) {
        fclose(f);
        return i;
      }
  }
  fclose(f);
  return 0;
}

/* ── read current EPP ───────────────────────────────── */
static int read_current_epp(void) {
  FILE *f = fopen(
      "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference",
      "r");
  if (!f)
    return 0;
  char line[32];
  if (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    for (int i = 0; i < epp_count; i++)
      if (!strcmp(line, epp_values[i])) {
        fclose(f);
        return i;
      }
  }
  fclose(f);
  return 0;
}

/* ── set governor (sysfs only, no sudo) ─────────────── */
static void set_governor(int idx) {
  if (idx < 0 || idx >= gov_count)
    return;
  FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
  if (f) {
    fprintf(f, "%s", gov_list[idx]);
    fclose(f);
    cur_gov = idx;
  }
}

/* ── set EPP (sysfs only, no sudo) ──────────────────── */
static void set_epp(int idx) {
  if (idx < 0 || idx >= epp_count)
    return;
  FILE *f = fopen(
      "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference",
      "w");
  if (f) {
    fprintf(f, "%s", epp_values[idx]);
    fclose(f);
    cur_epp = idx;
  }
}

/* ── module callbacks ─────────────────────────────────── */
static void cpu_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  if (!gov_count)
    load_available_governors();
  cur_gov = read_current_gov();
  cur_epp = read_current_epp();
}

static void cpu_draw(Module *m, int x, int y, int w, int h, int focused) {
  /* fill with governor colour */
  int gidx = (cur_gov >= 2) ? 1 : cur_gov;
  XColor bgc;
  XParseColor(dpy, DefaultColormap(dpy, screen), gov_colors[gidx], &bgc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bgc);
  XSetForeground(dpy, drw->gc, bgc.pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* EPP border – 4 px */
  XColor bdc;
  XParseColor(dpy, DefaultColormap(dpy, screen), epp_colors[cur_epp], &bdc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bdc);
  XSetForeground(dpy, drw->gc, bdc.pixel);
  for (int i = 0; i < 4; i++)
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                   h - 1 - 2 * i);

  /* thin black separator */
  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, x + 4, y + 4, w - 8, h - 8);

  /* governor character */
  const char *ch = gov_chars[gidx];
  XftColor *fg = &scheme[0][ColFg];
  Fnt *font = drw->fonts;
  int tw = drw_fontset_getwidth(drw, ch);
  int icon_x = x + (w - tw) / 2;
  int icon_y = y + (h - font->h) / 2 + font->xfont->ascent;

  XftDraw *xftdraw =
      XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen));
  XftDrawStringUtf8(xftdraw, fg, font->xfont, icon_x, icon_y,
                    (const XftChar8 *)ch, strlen(ch));
  XftDrawDestroy(xftdraw);
}

static void cpu_input(Module *m, const InputEvent *ev) {
  if (ev->type != EV_PRESS)
    return;
  if (ev->button == Button1) {
    int next = (cur_gov + 1) % gov_count;
    set_governor(next);
    panel_redraw();
  } else if (ev->button == Button2) {
    int next = (cur_epp + 1) % epp_count;
    set_epp(next);
    panel_redraw();
  }
}

static LayoutHints *cpu_hints(Module *m) {
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

Module cpu_governor_module = {
    .name = "cpu_governor",
    .init = cpu_init,
    .draw = cpu_draw,
    .input = cpu_input,
    .get_hints = cpu_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) cpu_governor_register(void) {
  register_module(&cpu_governor_module);
}
