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

/* ── power menu items ─────────────────────────────── */
static const char *pow_labels[] = {"Lock", "LogOff", "Sleep", "Reboot",
                                   "ShutDown"};
static const char *pow_cmds[] = {LOCK_CMD, LOGOUT_CMD, SLEEP_CMD, REBOOT_CMD,
                                 SHUTDOWN_CMD};
#define NPOWOPTS (int)(sizeof(pow_labels) / sizeof(pow_labels[0]))

/* ── per‑module state ─────────────────────────────── */
typedef struct {
  int active;        /* menu visible? */
  time_t start_time; /* when menu was opened */
} PowerState;

/* ── run a shell command ──────────────────────────── */
static void run_cmd(const char *cmd) {
  if (fork() == 0) {
    setsid();
    execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
    _exit(1);
  }
}

/* ── module callbacks ─────────────────────────────── */
static void power_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  PowerState *s = calloc(1, sizeof(PowerState));
  m->priv = s;
}

static void power_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  PowerState *s = (PowerState *)m->priv;

  /* thin border around the whole module (card style) */
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  int pad = MODULE_PADDING;
  int font_h = drw->fonts->h;

  if (!s->active) {
    /* ── idle: single centred bordered box with φ ── */
    const char *label = "φ";
    int label_w = drw_fontset_getwidth(drw, label);
    int box_w = label_w + 12;
    int box_h = h - 2 * pad;
    int box_x = x + (w - box_w) / 2;
    int box_y = y + pad;

    /* inner box border */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, box_x, box_y, box_w - 1,
                   box_h - 1);

    /* centred text */
    drw_setscheme(drw, scheme[0]);
    drw_text(drw, box_x + (box_w - label_w) / 2, box_y + (box_h - font_h) / 2,
             label_w, font_h, 0, label, 0);
    return;
  }

  /* ── expanded: 5 bordered boxes, evenly distributed ── */
  int item_w[NPOWOPTS];
  int total_w = 0;
  for (int i = 0; i < NPOWOPTS; i++) {
    item_w[i] = drw_fontset_getwidth(drw, pow_labels[i]) + 12;
    total_w += item_w[i];
  }
  int gap = 4;
  total_w += gap * (NPOWOPTS - 1);

  int start_x = x + (w - total_w) / 2;
  if (start_x < x + pad)
    start_x = x + pad;

  int box_h = h - 2 * pad;
  int box_y = y + pad;
  int cur_x = start_x;

  for (int i = 0; i < NPOWOPTS; i++) {
    /* border */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, cur_x, box_y, item_w[i] - 1,
                   box_h - 1);

    /* centred label */
    drw_setscheme(drw, scheme[0]);
    int label_w = drw_fontset_getwidth(drw, pow_labels[i]);
    drw_text(drw, cur_x + (item_w[i] - label_w) / 2,
             box_y + (box_h - font_h) / 2, label_w, font_h, 0, pow_labels[i],
             0);

    cur_x += item_w[i] + gap;
  }
}

static void power_input(Module *m, const InputEvent *ev) {
  PowerState *s = (PowerState *)m->priv;
  if (ev->type != EV_PRESS || ev->button != Button1)
    return;

  int rx = ev->x, ry = ev->y; /* module‑relative */
  int pad = MODULE_PADDING;

  if (!s->active) {
    /* idle → expand */
    s->active = 1;
    s->start_time = time(NULL);
    panel_redraw();
    return;
  }

  /* expanded → compute same geometry as draw */
  int item_w[NPOWOPTS];
  int total_w = 0;
  for (int i = 0; i < NPOWOPTS; i++) {
    item_w[i] = drw_fontset_getwidth(drw, pow_labels[i]) + 12;
    total_w += item_w[i];
  }
  int gap = 4;
  total_w += gap * (NPOWOPTS - 1);
  int start_x = (m->w - total_w) / 2;
  if (start_x < pad)
    start_x = pad;

  int box_h = m->h - 2 * pad;
  int box_y = pad;

  /* check if y is inside boxes at all */
  if (ry < box_y || ry >= box_y + box_h) {
    /* click outside boxes → collapse */
    s->active = 0;
    panel_redraw();
    return;
  }

  int cur_x = start_x;
  for (int i = 0; i < NPOWOPTS; i++) {
    if (rx >= cur_x && rx < cur_x + item_w[i]) {
      /* action chosen */
      run_cmd(pow_cmds[i]);
      s->active = 0;
      panel_redraw();
      return;
    }
    cur_x += item_w[i] + gap;
  }

  /* click inside the row but not on any box → collapse */
  s->active = 0;
  panel_redraw();
}

static void power_timer(Module *m) {
  PowerState *s = (PowerState *)m->priv;
  if (s->active && time(NULL) - s->start_time >= 10) {
    s->active = 0;
    panel_redraw();
  }
}

static void power_destroy(Module *m) { free(m->priv); }

static LayoutHints *power_hints(Module *m) {
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

Module power_module = {
    .name = "power",
    .init = power_init,
    .draw = power_draw,
    .input = power_input,
    .timer = power_timer,
    .destroy = power_destroy,
    .get_hints = power_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) power_register(void) {
  register_module(&power_module);
}
