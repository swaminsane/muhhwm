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

#define STATE_FILE "/tmp/pomodoro_state"

/* ── state we read from the script's state file ───── */
typedef struct {
  int active;
  int paused;
  int focus_sec;
  int break_sec;
  int total_cycles;
  int current_cycle;
  char phase[32];
  int end_or_rem; /* epoch if running, remaining secs if paused */
} PomoState;

/* ── read / parse the state file ──────────────────── */
static int read_pomo_state(PomoState *ps) {
  FILE *f = fopen(STATE_FILE, "r");
  if (!f) {
    ps->active = 0;
    return 0;
  }

  char line[256];
  if (!fgets(line, sizeof(line), f)) {
    fclose(f);
    ps->active = 0;
    return 0;
  }
  fclose(f);
  line[strcspn(line, "\n")] = '\0';

  char mode[32] = {0};
  int tot, cur, paused;
  if (sscanf(line, "%31[^|]|%d|%d|%d|%d|%31[^|]|%d|%d", mode, &ps->focus_sec,
             &ps->break_sec, &tot, &cur, ps->phase, &ps->end_or_rem,
             &paused) < 8) {
    ps->active = 0;
    return 0;
  }

  ps->total_cycles = tot;
  ps->current_cycle = cur;
  ps->paused = paused;
  ps->active = (strcmp(mode, "idle") != 0);
  return 1;
}

/* ── format remaining time ────────────────────────── */
static void format_remaining(PomoState *ps, char *buf, size_t sz) {
  int remaining;
  if (ps->paused) {
    remaining = ps->end_or_rem;
  } else {
    remaining = ps->end_or_rem - (int)time(NULL);
    if (remaining < 0)
      remaining = 0;
  }

  int m = remaining / 60;
  int s = remaining % 60;
  char cycle_str[32] = "";

  /* cycle count only when active */
  if (ps->active && ps->total_cycles != 0) {
    if (ps->total_cycles == -1)
      snprintf(cycle_str, sizeof(cycle_str), " [%d/∞]", ps->current_cycle);
    else
      snprintf(cycle_str, sizeof(cycle_str), " [%d/%d]", ps->current_cycle,
               ps->total_cycles);
  }

  if (ps->paused)
    snprintf(buf, sz, "Paused – %02dm %02ds%s", m, s, cycle_str);
  else if (strcmp(ps->phase, "Pomodoro") == 0)
    snprintf(buf, sz, "Focus – %02dm %02ds%s", m, s, cycle_str);
  else
    snprintf(buf, sz, "Break – %02dm %02ds%s", m, s, cycle_str);
}

/* ── module callbacks ─────────────────────────────── */
static void pom_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void pom_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  PomoState ps;
  if (!read_pomo_state(&ps))
    ps.active = 0;

  /* choose background colour from theme */
  const char *bgcol;
  if (!ps.active)
    bgcol = COL_BRIGHT_BLACK; /* dull / idle */
  else if (ps.paused)
    bgcol = COL_RED; /* paused */
  else if (strcmp(ps.phase, "Break") == 0)
    bgcol = COL_BLUE; /* break */
  else
    bgcol = COL_GREEN; /* focus */

  XColor bg;
  XParseColor(dpy, DefaultColormap(dpy, screen), bgcol, &bg);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bg);

  /* card background + border */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* coloured inner bar (leaves 2 px border on all sides) */
  XSetForeground(dpy, drw->gc, bg.pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x + 2, y + 2, w - 4, h - 4);

  /* thin border */
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  /* text – black colour */
  char text[64];
  if (!ps.active)
    strcpy(text, "Pomodoro – idle");
  else
    format_remaining(&ps, text, sizeof(text));

  int tw = drw_fontset_getwidth(drw, text);
  int font_h = drw->fonts->h;

  /* draw black text directly (no background) */
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
  XftDrawStringUtf8(xftdraw, &black, drw->fonts->xfont, x + (w - tw) / 2,
                    y + (h - font_h) / 2 + drw->fonts->xfont->ascent,
                    (const XftChar8 *)text, strlen(text));
  XftDrawDestroy(xftdraw);
  XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
               &black);
}

static void pom_input(Module *m, const InputEvent *ev) {
  if (ev->type != EV_PRESS)
    return;

  if (ev->button == Button1) {
    /* left click – toggle pause / resume (only if active) */
    PomoState ps;
    read_pomo_state(&ps);
    if (!ps.active)
      return;

    if (ps.paused)
      system("/home/swaminsane/.local/bin/menu/pomodmenu resume &");
    else
      system("/home/swaminsane/.local/bin/menu/pomodmenu pause &");
    panel_redraw();
  } else if (ev->button == Button3) {
    /* right click – open the interactive dmenu */
    system("/home/swaminsane/.local/bin/menu/pomodmenu &");
  } else if (ev->button == Button2) {
    /* middle click – stop the timer */
    system("/home/swaminsane/.local/bin/menu/pomodmenu stop &");
    panel_redraw();
  }
}

static void pom_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 1) {
    last = now;
    panel_redraw();
  }
}

static LayoutHints *pom_hints(Module *m) {
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

Module pomodoro_module = {
    .name = M_MID_POMODORO,
    .init = pom_init,
    .draw = pom_draw,
    .input = pom_input,
    .timer = pom_timer,
    .get_hints = pom_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) pomodoro_register(void) {
  register_module(&pomodoro_module);
}
