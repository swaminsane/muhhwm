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

typedef struct {
  char title[256];
  int playing;
  int position_sec;
  int duration_sec;
  int percentage;
  time_t last_poll;

  int dragging;
  int drag_start_x;
  int drag_start_val;

  /* hit‑area for "mpc" label */
  int mpc_x_start, mpc_x_end;
  int mpc_y_start, mpc_y_end;
} MusState;

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

static char *run_full(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return NULL;
  size_t cap = 256, len = 0;
  char *buf = malloc(cap);
  if (!buf) {
    pclose(f);
    return NULL;
  }
  buf[0] = '\0';
  char tmp[256];
  while (fgets(tmp, sizeof(tmp), f)) {
    size_t tlen = strlen(tmp);
    if (len + tlen + 1 > cap) {
      cap *= 2;
      char *newbuf = realloc(buf, cap);
      if (!newbuf) {
        free(buf);
        pclose(f);
        return NULL;
      }
      buf = newbuf;
    }
    strcpy(buf + len, tmp);
    len += tlen;
  }
  pclose(f);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = '\0';
  return buf;
}

/* ── poll mpc – robust parser ───────────────────── */
static void poll_mpc(MusState *s) {
  s->playing = 0;
  s->title[0] = '\0';
  s->position_sec = 0;
  s->duration_sec = 0;
  s->percentage = 0;

  char *full = run_full("mpc status 2>/dev/null");
  if (!full)
    return;

  char *save = full;
  char *line = strtok_r(save, "\n", &save);
  while (line) {
    if (strstr(line, "[playing]") || strstr(line, "[paused]")) {
      if (strstr(line, "[playing]"))
        s->playing = 1;

      char *p = line;
      while (*p && *p != ']')
        p++;
      if (*p == ']')
        p++;
      while (*p == ' ')
        p++;
      while (*p && *p != ' ')
        p++;
      while (*p == ' ')
        p++;

      char *time_start = p;
      while (*p && *p != ' ')
        p++;
      if (*p == ' ')
        *p++ = '\0';

      while (*p == ' ')
        p++;
      char *pct_start = p;
      if (*pct_start == '(')
        pct_start++;
      int pct = atoi(pct_start);
      if (pct >= 0 && pct <= 100)
        s->percentage = pct;

      char *slash = strchr(time_start, '/');
      if (slash) {
        *slash = '\0';
        char *elapsed = time_start;
        char *total = slash + 1;
        int em = 0, es = 0, tm = 0, ts = 0;
        if (sscanf(elapsed, "%d:%d", &em, &es) >= 1)
          s->position_sec = em * 60 + es;
        if (sscanf(total, "%d:%d", &tm, &ts) >= 1)
          s->duration_sec = tm * 60 + ts;
      }
      break;
    }
    line = strtok_r(NULL, "\n", &save);
  }
  free(full);

  char *title = run_line("mpc current 2>/dev/null");
  if (title) {
    strncpy(s->title, title, sizeof(s->title) - 1);
    free(title);
  }
}

static void fmt_time_sec(int sec, char *buf, size_t sz) {
  if (sec <= 0) {
    snprintf(buf, sz, "00:00");
    return;
  }
  snprintf(buf, sz, "%02d:%02d", sec / 60, sec % 60);
}

static unsigned long xpixel(const char *hex) {
  XColor col;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &col);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &col);
  return col.pixel;
}

/* ── module callbacks ──────────────────────────────── */
static void music_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  MusState *s = calloc(1, sizeof(MusState));
  m->priv = s;
  poll_mpc(s);
}

static void music_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  MusState *s = (MusState *)m->priv;

  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  if (!s->playing && s->title[0] == '\0') {
    drw_setscheme(drw, scheme[0]);
    const char *msg = "No MPC media";
    int tw = drw_fontset_getwidth(drw, msg);
    int fh = drw->fonts->h;
    drw_text(drw, x + (w - tw) / 2, y + (h - fh) / 2, tw, fh, 0, msg, 0);
    return;
  }

  int pad = MODULE_PADDING;
  int font_h = drw->fonts->h;
  int line_h = 24;
  int block_h = line_h * 2;
  int start_y = y + (h - block_h) / 2;
  if (start_y < y + pad)
    start_y = y + pad;

  int max_text_w = (w * 9) / 10;

  /* ── line 1: title    mpc (8px gap) ── */
  {
    char title_display[512];
    snprintf(title_display, sizeof(title_display), "%s",
             s->title[0] ? s->title : "unknown");
    drw_setscheme(drw, scheme[0]);
    int title_w = drw_fontset_getwidth(drw, title_display);
    const char *src_name = "mpc";
    int src_w = drw_fontset_getwidth(drw, src_name);
    int gap = 8; /* ← more breathing space */
    int avail = max_text_w - src_w - gap;
    if (title_w > avail)
      title_w = avail;
    drw_text(drw, x + pad, start_y, title_w, line_h, 0, title_display, 0);

    XftColor mpc_color;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), COL_BLUE, &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &mpc_color);
    }
    int src_x = x + pad + title_w + gap;
    int src_y = start_y + (line_h - font_h) / 2 + drw->fonts->xfont->ascent;
    XftDraw *xft = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xft, &mpc_color, drw->fonts->xfont, src_x, src_y,
                      (const XftChar8 *)src_name, strlen(src_name));
    XftDrawDestroy(xft);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &mpc_color);

    /* store hit‑area for "mpc" (module‑relative) */
    s->mpc_x_start = src_x;
    s->mpc_x_end = src_x + src_w;
    s->mpc_y_start = start_y;
    s->mpc_y_end = start_y + line_h;
  }

  /* ── line 2: square play/pause, time, progress, total ── */
  int bar_y = start_y + line_h;
  int bar_pad = 8;
  int bar_h = 10;
  int knob_size = 10;
  int bar_center_y = bar_y + line_h / 2 - bar_h / 2;

  int btn_size = line_h - 4;
  int btn_x = x + pad + bar_pad;
  int btn_y = bar_y + (line_h - btn_size) / 2;
  unsigned long accent = xpixel(COL_ACCENT);
  XSetForeground(dpy, drw->gc, accent);
  XFillRectangle(dpy, drw->drawable, drw->gc, btn_x, btn_y, btn_size, btn_size);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, btn_x, btn_y, btn_size - 1,
                 btn_size - 1);

  const char *icon = s->playing ? "|>" : "||";
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

  int bar_x = btn_x + btn_size + 4;

  char time_cur[16], time_tot[16];
  fmt_time_sec(s->position_sec, time_cur, sizeof(time_cur));
  fmt_time_sec(s->duration_sec, time_tot, sizeof(time_tot));
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

  double frac = s->percentage / 100.0;
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
  XFillRectangle(dpy, drw->drawable, drw->gc, knob_x + knob_size, bar_center_y,
                 track_end - (knob_x + knob_size), bar_h);

  int knob_y = bar_y + (line_h - knob_size) / 2;
  XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, knob_x, knob_y, knob_size,
                 knob_size);
}

static void music_input(Module *m, const InputEvent *ev) {
  MusState *s = (MusState *)m->priv;

  int pad = MODULE_PADDING;
  int line_h = 24;
  int bar_pad = 8;
  int btn_size = line_h - 4;
  int icon_start = pad + bar_pad;
  int icon_end = icon_start + btn_size + 4;

  int bar_x = icon_end;
  int tw_cur = drw_fontset_getwidth(drw, "00:00");
  bar_x += tw_cur + 4;
  int tw_tot = drw_fontset_getwidth(drw, "00:00");
  int total_x = m->w - pad - bar_pad - tw_tot;
  int track_start = bar_x;
  int track_end = total_x - 4;
  int track_len = track_end - track_start;
  if (track_len < 1)
    track_len = 1;

  int bar_y = pad + line_h;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    /* click on "mpc" label → open ncmpcpp */
    if (ev->y >= s->mpc_y_start && ev->y < s->mpc_y_end &&
        ev->x >= s->mpc_x_start && ev->x < s->mpc_x_end) {
      if (fork() == 0) {
        setsid();
        execlp("st", "st", "-e", "ncmpcpp", NULL);
        _exit(1);
      }
      return;
    }

    if (ev->y >= bar_y && ev->y < bar_y + line_h) {
      if (ev->x >= icon_start && ev->x < icon_end) {
        system("mpc toggle >/dev/null 2>&1");
        poll_mpc(s);
        panel_redraw();
        return;
      }
      if (ev->x >= track_start && ev->x <= track_end) {
        s->dragging = 1;
        s->drag_start_x = ev->x;
        s->drag_start_val = s->position_sec;
      }
    }
    return;
  }

  if (ev->type == EV_MOTION && s->dragging) {
    int clamped_x = ev->x;
    if (clamped_x < track_start)
      clamped_x = track_start;
    if (clamped_x > track_end)
      clamped_x = track_end;
    int dx = clamped_x - s->drag_start_x;

    if (s->duration_sec > 0 && track_len > 0) {
      int new_pos = s->drag_start_val + (dx * s->duration_sec) / track_len;
      if (new_pos < 0)
        new_pos = 0;
      if (new_pos > s->duration_sec)
        new_pos = s->duration_sec;

      int pct = (new_pos * 100) / s->duration_sec;
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "mpc seek %d%% >/dev/null 2>&1", pct);
      system(cmd);
      s->position_sec = new_pos;
      s->percentage = pct;
      panel_redraw();
    }
    return;
  }

  if (ev->type == EV_RELEASE && ev->button == Button1) {
    s->dragging = 0;
    poll_mpc(s);
    panel_redraw();
  }
}

static void music_timer(Module *m) {
  MusState *s = (MusState *)m->priv;
  time_t now = time(NULL);
  if (now - s->last_poll >= 1) {
    s->last_poll = now;
    if (!s->dragging)
      poll_mpc(s);
    panel_redraw();
  }
}

static void music_destroy(Module *m) { free(m->priv); }

static LayoutHints *music_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 56,
      .pref_h = 56,
      .max_h = 0,
      .expand_y = 1,
      .expand_x = 1,
  };
  return &hints;
}

Module music_module = {
    .name = "music",
    .init = music_init,
    .draw = music_draw,
    .input = music_input,
    .timer = music_timer,
    .destroy = music_destroy,
    .get_hints = music_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) music_register(void) {
  register_module(&music_module);
}
