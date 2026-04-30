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
  int volume; /* 0 … 100 */
  int muted;

  int dragging; /* 1 = dragging knob, 0 = idle */
  int drag_start_x;
  int drag_start_val;

  XftColor label_fg;
  Clr label_scheme[3];
} VolState;

static void alloc_xft_color(const char *hex, XftColor *dst) {
  XColor xc;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &xc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
  XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                     DefaultColormap(dpy, screen), &xrc, dst);
}

static void vol_get(int *vol, int *muted) {
  *vol = 0;
  *muted = 0;
  FILE *f = popen("amixer get Master 2>/dev/null | grep '%' | head -1 "
                  "| awk '{print $5}' | tr -d '[]%'",
                  "r");
  if (f) {
    if (fscanf(f, "%d", vol) != 1)
      *vol = 0;
    pclose(f);
  }
  f = popen(
      "amixer get Master 2>/dev/null | grep -q '\\[off\\]' && echo 1 || echo 0",
      "r");
  if (f) {
    if (fscanf(f, "%d", muted) != 1)
      *muted = 0;
    pclose(f);
  }
}

static void vol_set(int vol) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "amixer set Master %d%% >/dev/null 2>&1", vol);
  system(cmd);
}

static void vol_toggle_mute(void) {
  system("amixer set Master toggle >/dev/null 2>&1");
}

static void vol_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  VolState *s = calloc(1, sizeof(VolState));
  m->priv = s;
  vol_get(&s->volume, &s->muted);

  alloc_xft_color(COL_FG, &s->label_fg);
  s->label_scheme[ColFg] = s->label_fg;
  s->label_scheme[ColBg] = scheme[2][ColBg];
  s->label_scheme[ColBorder] = scheme[0][ColBorder];
}

static void vol_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  VolState *s = (VolState *)m->priv;

  /* card background + border */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  int pad = MODULE_PADDING;
  int font_h = drw->fonts->h;

  /* fixed label width = widest possible label (matches "T[10000K]") */
  const char *widest_label = "T[10000K]";
  int label_box_w = drw_fontset_getwidth(drw, widest_label);

  /* current label string, drawn centred inside that fixed‑width box */
  char label[32];
  snprintf(label, sizeof(label), "V[%d%%]", s->volume);
  int label_text_w = drw_fontset_getwidth(drw, label);
  int label_x = x + pad;
  int label_y = y + (h - font_h) / 2; /* vertically centred */
  drw_setscheme(drw, s->label_scheme);
  drw_text(drw, label_x + (label_box_w - label_text_w) / 2, label_y,
           label_text_w, font_h, 0, label, 0);

  /* track – gap of 12 px after the fixed‑width label box */
  int gap = 12;
  int track_start = label_x + label_box_w + gap;
  int track_end = x + w - pad - 6;
  int track_len = track_end - track_start;
  if (track_len < 16)
    track_len = 16;

  int bar_h = 6; /* thicker track */
  int knob_size = 10;
  int bar_center_y = y + h / 2 - bar_h / 2; /* perfectly centred */

  int knob_x =
      track_start + (int)(s->volume / 100.0f * track_len) - knob_size / 2;
  if (knob_x < track_start)
    knob_x = track_start;
  if (knob_x + knob_size > track_end)
    knob_x = track_end - knob_size;

  /* track colour */
  XColor tc;
  if (s->muted)
    XParseColor(dpy, DefaultColormap(dpy, screen), COL_BRIGHT_BLACK, &tc);
  else
    XParseColor(dpy, DefaultColormap(dpy, screen),
                s->volume > 70 ? COL_RED : COL_GREEN, &tc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &tc);
  XSetForeground(dpy, drw->gc, tc.pixel);

  /* filled part */
  XFillRectangle(dpy, drw->drawable, drw->gc, track_start, bar_center_y,
                 knob_x - track_start, bar_h);
  /* remaining part */
  XSetForeground(dpy, drw->gc, 0x3b4252);
  XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + knob_size, bar_center_y,
                 track_end - (knob_x + knob_size), bar_h);

  /* knob (centred on the track) */
  int knob_y = bar_center_y + bar_h / 2 - knob_size / 2;
  XSetForeground(dpy, drw->gc, s->label_fg.pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + 1, knob_y + 1,
                 knob_size - 2, knob_size - 2);
  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, knob_x, knob_y, knob_size - 1,
                 knob_size - 1);
}

static void vol_input(Module *m, const InputEvent *ev) {
  VolState *s = (VolState *)m->priv;
  if (ev->type != EV_PRESS && ev->type != EV_RELEASE && ev->type != EV_MOTION)
    return;

  int pad = MODULE_PADDING;
  /* track geometry (same as draw) */
  const char *widest_label = "T[10000K]";
  int label_box_w = drw_fontset_getwidth(drw, widest_label);
  int gap = 12;
  int track_start = m->x + pad + label_box_w + gap;
  int track_end = m->x + m->w - pad - 6;
  int track_len = track_end - track_start;
  if (track_len < 1)
    track_len = 1;

  if (ev->type == EV_PRESS) {
    if (ev->button == Button3) {
      /* right click – open pavucontrol and close panel */
      if (fork() == 0) {
        setsid();
        execlp("pavucontrol", "pavucontrol", NULL);
        _exit(1);
      }
      panel_hide();
      return;
    }
    if (ev->button == Button2) {
      /* middle click – toggle mute */
      vol_toggle_mute();
      vol_get(&s->volume, &s->muted);
      panel_redraw();
      return;
    }
    if (ev->button == Button1) {
      int press_x = ev->root_x;
      if (press_x >= track_start && press_x <= track_end) {
        s->dragging = 1;
        s->drag_start_x = press_x;
        s->drag_start_val = s->volume;
      }
      /* left click outside track does nothing */
      return;
    }
  }

  if (ev->type == EV_MOTION && s->dragging) {
    int mod_abs_x = panel_x + m->x;
    int mod_abs_y = panel_y + m->y;
    if (ev->root_x < mod_abs_x || ev->root_x >= mod_abs_x + m->w ||
        ev->root_y < mod_abs_y || ev->root_y >= mod_abs_y + m->h) {
      s->dragging = 0;
      return;
    }

    int clamped_x = ev->root_x;
    if (clamped_x < track_start)
      clamped_x = track_start;
    if (clamped_x > track_end)
      clamped_x = track_end;

    int dx = clamped_x - s->drag_start_x;
    int new_val = s->drag_start_val + (dx * 100) / track_len;
    new_val = ((new_val + 1) / 2) * 2; /* snap to even */
    if (new_val < 0)
      new_val = 0;
    if (new_val > 100)
      new_val = 100;
    if (new_val != s->volume) {
      vol_set(new_val);
      s->volume = new_val;
      panel_redraw();
    }
    return;
  }

  if (ev->type == EV_RELEASE && ev->button == Button1) {
    s->dragging = 0;
  }
}

static void vol_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 1) {
    last = now;
    VolState *s = (VolState *)m->priv;
    int old_v = s->volume, old_m = s->muted;
    vol_get(&s->volume, &s->muted);
    if (s->volume != old_v || s->muted != old_m)
      panel_redraw();
  }
}

static void vol_destroy(Module *m) {
  VolState *s = (VolState *)m->priv;
  if (s) {
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &s->label_fg);
    free(s);
  }
}

static LayoutHints *vol_hints(Module *m) {
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

Module volume_module = {
    .name = "volume",
    .init = vol_init,
    .draw = vol_draw,
    .input = vol_input,
    .timer = vol_timer,
    .destroy = vol_destroy,
    .get_hints = vol_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) volume_register(void) {
  register_module(&volume_module);
}
