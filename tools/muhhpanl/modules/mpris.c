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

#define MAX_SOURCES 4

/* ── source record ───────────────────────────────── */
typedef struct {
  char instance[256]; /* full MPRIS instance name (for commands) */
  char title[256];
  char player_name[64]; /* short display name from metadata */
  int playing;
  long long position_us; /* microseconds */
  long long duration_us;
  int percentage;

  /* hit‑areas for per‑source widgets */
  int src_x_start, src_x_end, src_y_start, src_y_end;
  int btn_x_start, btn_x_end, btn_y_start, btn_y_end;
  int track_x_start, track_x_end;
  int bar_y_start, bar_y_end;
} Source;

/* ── module state ────────────────────────────────── */
typedef struct {
  Source sources[MAX_SOURCES];
  int nactive;

  int dragging;             /* index of dragged source, or -1 */
  int drag_start_x;         /* module‑relative */
  long long drag_start_pos; /* position_us when drag started */
  time_t last_poll;
} MprisState;

/* ── helpers ────────────────────────────────────── */
static char *run_line(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return NULL;
  static char buf[512];
  if (fgets(buf, sizeof(buf), f)) {
    pclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return strdup(buf);
  }
  pclose(f);
  return NULL;
}

static unsigned long xpixel(const char *hex) {
  XColor col;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &col);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &col);
  return col.pixel;
}

static const char *player_color(const char *name) {
  if (!name)
    return COL_BRIGHT_BLACK;
  char lower[64];
  strncpy(lower, name, sizeof(lower) - 1);
  for (int i = 0; lower[i]; i++)
    if (lower[i] >= 'A' && lower[i] <= 'Z')
      lower[i] += 32;
  if (strstr(lower, "firefox"))
    return COL_YELLOW;
  if (strstr(lower, "mpv"))
    return COL_GREEN;
  if (strstr(lower, "vlc"))
    return COL_ACCENT;
  if (strstr(lower, "spotify"))
    return COL_GREEN;
  if (strstr(lower, "chromium") || strstr(lower, "chrome"))
    return COL_YELLOW;
  return COL_BRIGHT_BLACK;
}

/* ── poll one MPRIS player ──────────────────────── */
static int poll_mpris(Source *s, const char *instance) {
  char cmd[512];

  /* skip plasma-browser-integration (duplicate) */
  if (strstr(instance, "plasma-browser-integration"))
    return 0;

  /* check status */
  snprintf(cmd, sizeof(cmd), "/usr/bin/playerctl -i \"%s\" status 2>/dev/null",
           instance);
  char *status = run_line(cmd);
  if (!status || strcmp(status, "Playing") != 0) {
    free(status);
    return 0;
  }
  free(status);

  strncpy(s->instance, instance, sizeof(s->instance) - 1);
  s->playing = 1;

  /* title */
  snprintf(cmd, sizeof(cmd),
           "/usr/bin/playerctl -i \"%s\" metadata --format '{{ title }}' "
           "2>/dev/null",
           instance);
  char *title = run_line(cmd);
  strncpy(s->title, title ? title : "unknown", sizeof(s->title) - 1);
  free(title);

  /* player name (short) – always from metadata */
  snprintf(cmd, sizeof(cmd),
           "/usr/bin/playerctl -i \"%s\" metadata --format '{{ playerName }}' "
           "2>/dev/null",
           instance);
  char *pname = run_line(cmd);
  if (pname && pname[0]) {
    strncpy(s->player_name, pname, sizeof(s->player_name) - 1);
    free(pname);
  } else {
    free(pname);
    /* fallback: part before first dot */
    char *dot = strchr(instance, '.');
    if (dot)
      snprintf(s->player_name, sizeof(s->player_name), "%.*s",
               (int)(dot - instance), instance);
    else
      strncpy(s->player_name, instance, sizeof(s->player_name) - 1);
  }

  /* position */
  snprintf(cmd, sizeof(cmd),
           "/usr/bin/playerctl -i \"%s\" position 2>/dev/null", instance);
  char *pos = run_line(cmd);
  s->position_us = pos ? atoll(pos) : 0;
  free(pos);

  /* duration */
  snprintf(cmd, sizeof(cmd),
           "/usr/bin/playerctl -i \"%s\" metadata mpris:length 2>/dev/null",
           instance);
  char *dur = run_line(cmd);
  s->duration_us = dur ? atoll(dur) : 0;
  free(dur);

  /* percentage */
  if (s->duration_us > 0)
    s->percentage = (int)((s->position_us * 100) / s->duration_us);
  else
    s->percentage = 0;

  return 1;
}

/* ── refresh all sources ────────────────────────── */
static void poll_all(MprisState *s) {
  s->nactive = 0;

  char *list = run_line("/usr/bin/playerctl --list-all 2>/dev/null");
  if (!list)
    return;

  char *saveptr = NULL;
  char *tok = strtok_r(list, "\n", &saveptr);
  while (tok && s->nactive < MAX_SOURCES) {
    while (*tok == ' ' || *tok == '\t')
      tok++;
    if (*tok == '\0') {
      tok = strtok_r(NULL, "\n", &saveptr);
      continue;
    }
    if (poll_mpris(&s->sources[s->nactive], tok))
      s->nactive++;
    tok = strtok_r(NULL, "\n", &saveptr);
  }
  free(list);
}

/* ── format microseconds to mm:ss ──────────── */
static void fmt_time_us(long long us, char *buf, size_t sz) {
  if (us <= 0) {
    snprintf(buf, sz, "00:00");
    return;
  }
  long long sec = us / 1000000;
  snprintf(buf, sz, "%02lld:%02lld", sec / 60, sec % 60);
}

/* ── open app for a source ─────────────────────── */
static void open_source_app(const char *player_name) {
  if (strcmp(player_name, "firefox") == 0) {
    system("firefox --new-window about:blank &");
  } else if (strcmp(player_name, "vlc") == 0) {
    system("vlc &");
  }
}

/* ── module callbacks ──────────────────────────────── */
static void mpris_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  MprisState *s = calloc(1, sizeof(MprisState));
  m->priv = s;
  s->dragging = -1;
  poll_all(s);
}

static void mpris_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  MprisState *s = (MprisState *)m->priv;

  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  if (s->nactive == 0) {
    drw_setscheme(drw, scheme[0]);
    const char *msg = "No media";
    int tw = drw_fontset_getwidth(drw, msg);
    int fh = drw->fonts->h;
    drw_text(drw, x + (w - tw) / 2, y + (h - fh) / 2, tw, fh, 0, msg, 0);
    return;
  }

  int pad = MODULE_PADDING;
  int font_h = drw->fonts->h;
  int line_h = 24;
  int per_source_h = line_h * 2;
  int block_h = per_source_h * s->nactive;
  int start_y = y + (h - block_h) / 2;
  if (start_y < y + pad)
    start_y = y + pad;

  int max_text_w = (w * 9) / 10;

  for (int i = 0; i < s->nactive; i++) {
    Source *src = &s->sources[i];
    int src_y = start_y + i * per_source_h;

    /* ── line 1: title   source_name ── */
    {
      char title_display[512];
      snprintf(title_display, sizeof(title_display), "%s",
               src->title[0] ? src->title : "unknown");
      drw_setscheme(drw, scheme[0]);
      int title_w = drw_fontset_getwidth(drw, title_display);
      const char *sname = src->player_name;
      int sw = drw_fontset_getwidth(drw, sname);
      int gap = 8;
      int avail = max_text_w - sw - gap;
      if (title_w > avail)
        title_w = avail;
      drw_text(drw, x + pad, src_y, title_w, line_h, 0, title_display, 0);

      const char *pcol = player_color(sname);
      XftColor sc;
      {
        XColor xc;
        XParseColor(dpy, DefaultColormap(dpy, screen), pcol, &xc);
        XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
        XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
        XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                           DefaultColormap(dpy, screen), &xrc, &sc);
      }
      int sx = x + pad + title_w + gap;
      int sy = src_y + (line_h - font_h) / 2 + drw->fonts->xfont->ascent;
      XftDraw *xft =
          XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                        DefaultColormap(dpy, screen));
      XftDrawStringUtf8(xft, &sc, drw->fonts->xfont, sx, sy,
                        (const XftChar8 *)sname, strlen(sname));
      XftDrawDestroy(xft);
      XftColorFree(dpy, DefaultVisual(dpy, screen),
                   DefaultColormap(dpy, screen), &sc);

      src->src_x_start = sx;
      src->src_x_end = sx + sw;
      src->src_y_start = src_y;
      src->src_y_end = src_y + line_h;
    }

    /* ── line 2: square play/pause, time, progress, total ── */
    int bar_y = src_y + line_h;
    int bar_pad = 8;
    int bar_h = 10;
    int knob_size = 10;
    int bar_center_y = bar_y + line_h / 2 - bar_h / 2;

    int btn_size = line_h - 4;
    int btn_x = x + pad + bar_pad;
    int btn_y = bar_y + (line_h - btn_size) / 2;
    unsigned long accent = xpixel(COL_ACCENT);
    XSetForeground(dpy, drw->gc, accent);
    XFillRectangle(dpy, drw->drawable, drw->gc, btn_x, btn_y, btn_size,
                   btn_size);
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, btn_x, btn_y, btn_size - 1,
                   btn_size - 1);

    const char *icon = src->playing ? "|>" : "||";
    int icon_w = drw_fontset_getwidth(drw, icon);
    XftColor icon_fg;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), COL_BG, &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &icon_fg);
    }
    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    int icon_x = btn_x + (btn_size - icon_w) / 2;
    int icon_y = btn_y + (btn_size - font_h) / 2 + drw->fonts->xfont->ascent;
    XftDrawStringUtf8(xft, &icon_fg, drw->fonts->xfont, icon_x, icon_y,
                      (const XftChar8 *)icon, strlen(icon));
    XftDrawDestroy(xft);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &icon_fg);

    src->btn_x_start = btn_x;
    src->btn_x_end = btn_x + btn_size;
    src->btn_y_start = btn_y;
    src->btn_y_end = btn_y + btn_size;

    int bar_x = btn_x + btn_size + 4;

    char time_cur[16], time_tot[16];
    fmt_time_us(src->position_us, time_cur, sizeof(time_cur));
    fmt_time_us(src->duration_us, time_tot, sizeof(time_tot));
    int tw_cur = drw_fontset_getwidth(drw, time_cur);
    drw_setscheme(drw, scheme[0]);
    drw_text(drw, bar_x, bar_y, tw_cur, line_h, 0, time_cur, 0);
    bar_x += tw_cur + 4;

    int tw_tot = drw_fontset_getwidth(drw, time_tot);
    int total_x = x + w - pad - bar_pad - tw_tot;
    drw_text(drw, total_x, bar_y, tw_tot, line_h, 0, time_tot, 0);

    int track_start = bar_x;
    int track_end = total_x - 4;
    int track_len = track_end - track_start;
    if (track_len < 16)
      track_len = 16;

    double frac = src->percentage / 100.0;
    if (frac > 1.0)
      frac = 1.0;
    if (frac < 0.0)
      frac = 0.0;
    int knob_x = track_start + (int)(frac * track_len) - knob_size / 2;
    if (knob_x < track_start)
      knob_x = track_start;
    if (knob_x + knob_size > track_end)
      knob_x = track_end - knob_size;

    unsigned long green = xpixel(COL_GREEN);
    XSetForeground(dpy, drw->gc, green);
    XFillRectangle(dpy, drw->drawable, drw->gc, track_start, bar_center_y,
                   knob_x - track_start, bar_h);

    XSetForeground(dpy, drw->gc, 0x3b4252);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + knob_size,
                   bar_center_y, track_end - (knob_x + knob_size), bar_h);

    int knob_y = bar_y + (line_h - knob_size) / 2;
    XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, knob_x, knob_y, knob_size,
                   knob_size);

    src->track_x_start = track_start;
    src->track_x_end = track_end;
    src->bar_y_start = bar_y;
    src->bar_y_end = bar_y + line_h;
  }
}

/* ── input ──────────────────────────────────────── */
static void mpris_input(Module *m, const InputEvent *ev) {
  MprisState *s = (MprisState *)m->priv;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    for (int i = 0; i < s->nactive; i++) {
      Source *src = &s->sources[i];

      if (ev->y >= src->src_y_start && ev->y < src->src_y_end &&
          ev->x >= src->src_x_start && ev->x < src->src_x_end) {
        open_source_app(src->player_name);
        return;
      }

      if (ev->y >= src->btn_y_start && ev->y < src->btn_y_end &&
          ev->x >= src->btn_x_start && ev->x < src->btn_x_end) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "/usr/bin/playerctl -i \"%s\" play-pause 2>/dev/null",
                 src->instance);
        system(cmd);
        poll_all(s);
        panel_redraw();
        return;
      }

      if (ev->y >= src->bar_y_start && ev->y < src->bar_y_end &&
          ev->x >= src->track_x_start && ev->x <= src->track_x_end) {
        s->dragging = i;
        s->drag_start_x = ev->x;
        s->drag_start_pos = src->position_us;
        return;
      }
    }
    return;
  }

  if (ev->type == EV_MOTION && s->dragging >= 0) {
    Source *src = &s->sources[s->dragging];
    int tx = src->track_x_start;
    int te = src->track_x_end;
    int tlen = te - tx;
    if (tlen < 1)
      tlen = 1;

    int clamped_x = ev->x;
    if (clamped_x < tx)
      clamped_x = tx;
    if (clamped_x > te)
      clamped_x = te;
    int dx = clamped_x - s->drag_start_x;

    if (src->duration_us > 0) {
      long long new_pos = s->drag_start_pos + (dx * src->duration_us) / tlen;
      if (new_pos < 0)
        new_pos = 0;
      if (new_pos > src->duration_us)
        new_pos = src->duration_us;

      char cmd[512];
      snprintf(cmd, sizeof(cmd),
               "/usr/bin/playerctl -i \"%s\" position %lld 2>/dev/null",
               src->instance, new_pos);
      system(cmd);
      src->position_us = new_pos;
      src->percentage = (int)((new_pos * 100) / src->duration_us);
      panel_redraw();
    }
    return;
  }

  if (ev->type == EV_RELEASE && ev->button == Button1) {
    s->dragging = -1;
    poll_all(s);
    panel_redraw();
  }
}

static void mpris_timer(Module *m) {
  MprisState *s = (MprisState *)m->priv;
  time_t now = time(NULL);
  if (now - s->last_poll >= 1) {
    s->last_poll = now;
    if (s->dragging < 0)
      poll_all(s);
    panel_redraw();
  }
}

static void mpris_destroy(Module *m) { free(m->priv); }

static LayoutHints *mpris_hints(Module *m) {
  (void)m;
  MprisState *s = (MprisState *)m->priv;
  int height = 56;
  if (s) {
    height = 56 * s->nactive;
    if (height < 56)
      height = 56;
  }
  static LayoutHints hints;
  hints.min_h = height;
  hints.pref_h = height;
  hints.max_h = 56 * MAX_SOURCES;
  hints.expand_y = 1;
  hints.expand_x = 1;
  return &hints;
}

Module mpris_module = {
    .name = M_MID_MPRIS,
    .init = mpris_init,
    .draw = mpris_draw,
    .input = mpris_input,
    .timer = mpris_timer,
    .destroy = mpris_destroy,
    .get_hints = mpris_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) mpris_register(void) {
  register_module(&mpris_module);
}
