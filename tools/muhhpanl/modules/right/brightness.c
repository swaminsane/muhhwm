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

#define TEMP_FILE "/tmp/xsct_temp"
#define TEMP_MIN 1000
#define TEMP_MAX 10000
#define TEMP_STEP 100
#define TEMP_DEFAULT 4200

typedef struct {
  int brightness;  /* 0 … 100 */
  int temperature; /* 1000 … 10000 K */

  int dragging; /* 0=none, 1=brightness, 2=temperature */
  int drag_start_x;
  int drag_start_val;

  XftColor label_color; /* allocated from COL_FG */
} BriState;

static void alloc_xft_color(const char *hex, XftColor *dst) {
  XColor xc;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &xc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
  XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
  XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                     DefaultColormap(dpy, screen), &xrc, dst);
}

static unsigned long alloc_rgb(int r, int g, int b) {
  XColor col;
  col.red = r * 257;
  col.green = g * 257;
  col.blue = b * 257;
  col.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(dpy, DefaultColormap(dpy, screen), &col);
  return col.pixel;
}

static void lerp_rgb(int r1, int g1, int b1, int r2, int g2, int b2, float t,
                     int *r, int *g, int *b) {
  *r = r1 + (r2 - r1) * t;
  *g = g1 + (g2 - g1) * t;
  *b = b1 + (b2 - b1) * t;
}

static unsigned long temp_track_color(int kelvin) {
  float t = (float)(kelvin - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  int r, g, b;
  if (t <= 0.5f) {
    float s = t / 0.5f;
    lerp_rgb(255, 80, 80, 255, 255, 255, s, &r, &g, &b);
  } else {
    float s = (t - 0.5f) / 0.5f;
    lerp_rgb(255, 255, 255, 80, 80, 255, s, &r, &g, &b);
  }
  return alloc_rgb(r, g, b);
}

static int get_brightness(void) {
  FILE *f = popen("brightnessctl g 2>/dev/null", "r");
  if (!f)
    return 50;
  int cur = 0;
  if (fscanf(f, "%d", &cur) != 1)
    cur = 50;
  pclose(f);
  f = popen("brightnessctl m 2>/dev/null", "r");
  if (!f)
    return 50;
  int max = 1;
  if (fscanf(f, "%d", &max) != 1)
    max = 1;
  pclose(f);
  return (cur * 100) / max;
}

static void set_brightness(int pct) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "brightnessctl s %d%% >/dev/null 2>&1", pct);
  system(cmd);
}

static int get_temperature(void) {
  FILE *f = fopen(TEMP_FILE, "r");
  if (!f)
    return TEMP_DEFAULT;
  int t = TEMP_DEFAULT;
  if (fscanf(f, "%d", &t) != 1)
    t = TEMP_DEFAULT;
  fclose(f);
  if (t < TEMP_MIN)
    t = TEMP_MIN;
  if (t > TEMP_MAX)
    t = TEMP_MAX;
  return t;
}

static void set_temperature(int kelvin) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "xsct %d", kelvin);
  system(cmd);
  FILE *f = fopen(TEMP_FILE, "w");
  if (f) {
    fprintf(f, "%d\n", kelvin);
    fclose(f);
  }
}

/* ── module callbacks ───────────────────────────────── */
static void bri_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  BriState *s = calloc(1, sizeof(BriState));
  m->priv = s;
  s->brightness = get_brightness();
  s->temperature = get_temperature();
  set_temperature(s->temperature);
  alloc_xft_color(COL_FG, &s->label_color);
}

static void bri_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  BriState *s = (BriState *)m->priv;

  /* card background + border */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  int pad = MODULE_PADDING;
  int font_h = drw->fonts->h;
  Fnt *font = drw->fonts;
  const char *widest_label = "T[10000K]";
  int max_label_w = drw_fontset_getwidth(drw, widest_label);
  const int block_h = 50;
  int block_start_y = y + (h - block_h) / 2; /* centre vertically */

  /* brightness */
  {
    int br_y = block_start_y, br_h = 24;
    char label[32];
    snprintf(label, sizeof(label), "B[%d%%]", s->brightness);
    int label_y = br_y + (br_h - font_h) / 2 + font->xfont->ascent;
    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xft, &s->label_color, font->xfont, x + pad, label_y,
                      (const XftChar8 *)label, strlen(label));
    XftDrawDestroy(xft);

    int track_start = x + pad + max_label_w + 8;
    int track_end = x + w - pad - 6;
    int track_len = track_end - track_start;
    if (track_len < 16)
      track_len = 16;
    int knob_size = 10;
    int knob_x =
        track_start + (s->brightness / 100.0f * track_len) - (knob_size / 2);
    if (knob_x < track_start)
      knob_x = track_start;
    if (knob_x + knob_size > track_end)
      knob_x = track_end - knob_size;

    XColor tc;
    XParseColor(dpy, DefaultColormap(dpy, screen),
                s->brightness > 50 ? COL_RED : COL_GREEN, &tc);
    XAllocColor(dpy, DefaultColormap(dpy, screen), &tc);
    XSetForeground(dpy, drw->gc, tc.pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, track_start,
                   br_y + br_h / 2 - 1, knob_x - track_start, 2);
    XSetForeground(dpy, drw->gc, 0x3b4252);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + knob_size,
                   br_y + br_h / 2 - 1, track_end - (knob_x + knob_size), 2);

    int knob_y = br_y + (br_h - knob_size) / 2;
    XSetForeground(dpy, drw->gc, s->label_color.pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + 1, knob_y + 1,
                   knob_size - 2, knob_size - 2);
    XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
    XDrawRectangle(dpy, drw->drawable, drw->gc, knob_x, knob_y, knob_size - 1,
                   knob_size - 1);
  }

  /* temperature */
  {
    int tp_y = block_start_y + 26, tp_h = 24;
    char label[32];
    snprintf(label, sizeof(label), "T[%dK]", s->temperature);
    int label_y = tp_y + (tp_h - font_h) / 2 + font->xfont->ascent;
    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xft, &s->label_color, font->xfont, x + pad, label_y,
                      (const XftChar8 *)label, strlen(label));
    XftDrawDestroy(xft);

    int track_start = x + pad + max_label_w + 8;
    int track_end = x + w - pad - 6;
    int track_len = track_end - track_start;
    if (track_len < 16)
      track_len = 16;
    int knob_size = 10;
    float frac = (float)(s->temperature - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
    int knob_x = track_start + (int)(frac * track_len) - (knob_size / 2);
    if (knob_x < track_start)
      knob_x = track_start;
    if (knob_x + knob_size > track_end)
      knob_x = track_end - knob_size;

    unsigned long tcol = temp_track_color(s->temperature);
    XSetForeground(dpy, drw->gc, tcol);
    XFillRectangle(dpy, drw->drawable, drw->gc, track_start,
                   tp_y + tp_h / 2 - 1, knob_x - track_start, 2);
    XSetForeground(dpy, drw->gc, 0x3b4252);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + knob_size,
                   tp_y + tp_h / 2 - 1, track_end - (knob_x + knob_size), 2);

    int knob_y = tp_y + (tp_h - knob_size) / 2;
    XSetForeground(dpy, drw->gc, s->label_color.pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + 1, knob_y + 1,
                   knob_size - 2, knob_size - 2);
    XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
    XDrawRectangle(dpy, drw->drawable, drw->gc, knob_x, knob_y, knob_size - 1,
                   knob_size - 1);
  }
}

// … (everything before bri_input stays the same) …

static void bri_input(Module *m, const InputEvent *ev) {
  BriState *s = (BriState *)m->priv;
  if (ev->type != EV_PRESS && ev->type != EV_RELEASE && ev->type != EV_MOTION)
    return;

  int pad = MODULE_PADDING;
  int h = m->h;
  const int block_h = 50;
  int block_start_y = m->y + (h - block_h) / 2;

  int br_y = block_start_y;
  int tp_y = block_start_y + 26;

  int slider = 0;
  if (ev->root_y - panel_y >= br_y && ev->root_y - panel_y < br_y + 24)
    slider = 1;
  else if (ev->root_y - panel_y >= tp_y && ev->root_y - panel_y < tp_y + 24)
    slider = 2;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    if (slider == 0)
      return;
    s->dragging = slider;
    s->drag_start_x = ev->root_x;
    s->drag_start_val = (slider == 1) ? s->brightness : s->temperature;
    return;
  }

  if (ev->type == EV_MOTION && s->dragging) {
    /* Cancel drag if cursor left the module entirely */
    int mod_abs_x = panel_x + m->x;
    int mod_abs_y = panel_y + m->y;
    if (ev->root_x < mod_abs_x || ev->root_x >= mod_abs_x + m->w ||
        ev->root_y < mod_abs_y || ev->root_y >= mod_abs_y + m->h) {
      s->dragging = 0;
      return;
    }

    int max_label_w = drw_fontset_getwidth(drw, "T[10000K]");
    int track_start = m->x + pad + max_label_w + 8;
    int track_end = m->x + m->w - pad - 6;
    int track_len = track_end - track_start;
    if (track_len < 1)
      track_len = 1;

    int clamped_x = ev->root_x;
    if (clamped_x < track_start)
      clamped_x = track_start;
    if (clamped_x > track_end)
      clamped_x = track_end;

    int dx = clamped_x - s->drag_start_x;

    if (s->dragging == 1) {
      int new_val = s->drag_start_val + (dx * 100) / track_len;
      new_val = ((new_val + 1) / 2) * 2;
      if (new_val < 0)
        new_val = 0;
      if (new_val > 100)
        new_val = 100;
      if (new_val != s->brightness) {
        set_brightness(new_val);
        s->brightness = new_val;
        panel_redraw();
      }
    } else if (s->dragging == 2) {
      int new_val =
          s->drag_start_val + (dx * (TEMP_MAX - TEMP_MIN)) / track_len;
      new_val = ((new_val + TEMP_STEP / 2) / TEMP_STEP) * TEMP_STEP;
      if (new_val < TEMP_MIN)
        new_val = TEMP_MIN;
      if (new_val > TEMP_MAX)
        new_val = TEMP_MAX;
      if (new_val != s->temperature) {
        set_temperature(new_val);
        s->temperature = new_val;
        panel_redraw();
      }
    }
    return;
  }

  // … (release remains unchanged) …
}

static void bri_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 2) {
    last = now;
    BriState *s = (BriState *)m->priv;
    int old_b = s->brightness, old_t = s->temperature;
    s->brightness = get_brightness();
    s->temperature = get_temperature();
    if (s->brightness != old_b || s->temperature != old_t)
      panel_redraw();
  }
}

static void bri_destroy(Module *m) {
  BriState *s = (BriState *)m->priv;
  if (s) {
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &s->label_color);
    free(s);
  }
}

static LayoutHints *bri_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 50,
      .pref_h = 50,
      .max_h = 50,
      .expand_y = 0,
      .expand_x = 1,
  };
  return &hints;
}

Module brightness_module = {
    .name = M_RIGHT_BRI,
    .init = bri_init,
    .draw = bri_draw,
    .input = bri_input,
    .timer = bri_timer,
    .destroy = bri_destroy,
    .get_hints = bri_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) brightness_register(void) {
  register_module(&brightness_module);
}
