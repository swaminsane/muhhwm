#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
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
#define SCRIPT_PATH "/home/swaminsane/.local/bin/mpris-ctl"
#define BOX_GAP 4 /* vertical gap between two visible boxes */

/* ── per‑player record ─────────────────────────── */
typedef struct {
  char bus[256];
  char title[256];
  char artist[256];
  char player_name[64];
  int playing; /* 1 = Playing, 0 = Paused */
  int position_sec;
  int duration_sec;
  int percentage;

  /* hit‑areas (content‑relative, for one box) */
  int src_x1, src_x2, src_y1, src_y2;
  int btn_x1, btn_x2, btn_y1, btn_y2;
  int trk_x1, trk_x2, trk_y1, trk_y2;
} Source;

/* ── module state ──────────────────────────────── */
typedef struct {
  Source src[MAX_SOURCES];
  int n; /* total active sources */
  int dragging;
  int drag_start_x;
  int drag_start_pct;
  int scroll_offset;
  int initialized; /* becomes 1 after first draw */
  int need_relayout;
  time_t last_poll;
} MprisState;

/* ── helpers ───────────────────────────────────── */
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

static char *run_all(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return NULL;
  size_t cap = 1024, len = 0;
  char *buf = malloc(cap);
  if (!buf) {
    pclose(f);
    return NULL;
  }
  buf[0] = '\0';
  char tmp[256];
  while (fgets(tmp, sizeof(tmp), f)) {
    size_t add = strlen(tmp);
    if (len + add + 1 > cap) {
      cap *= 2;
      char *nb = realloc(buf, cap);
      if (!nb) {
        free(buf);
        pclose(f);
        return NULL;
      }
      buf = nb;
    }
    memcpy(buf + len, tmp, add + 1);
    len += add;
  }
  pclose(f);
  return buf;
}

static unsigned long xpixel(const char *hex) {
  XColor c;
  XParseColor(dpy, DefaultColormap(dpy, screen), hex, &c);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &c);
  return c.pixel;
}

static const char *player_color(const char *name) {
  if (!name)
    return COL_BRIGHT_BLACK;
  char lo[64];
  strncpy(lo, name, 63);
  lo[63] = '\0';
  for (int i = 0; lo[i]; i++)
    if (lo[i] >= 'A' && lo[i] <= 'Z')
      lo[i] += 32;
  if (strstr(lo, "firefox"))
    return COL_YELLOW;
  if (strstr(lo, "mpv"))
    return COL_GREEN;
  if (strstr(lo, "spotify"))
    return COL_GREEN;
  if (strstr(lo, "vlc"))
    return COL_ACCENT;
  return COL_BRIGHT_BLACK;
}

/* Parse "MM:SS/MM:SS|PP" or "H:MM:SS/H:MM:SS|PP" */
static void parse_progress(const char *s, int *pos, int *dur, int *pct) {
  *pos = *dur = *pct = 0;
  if (!s)
    return;
  int ph, pm, ps, dh, dm, ds, pp;
  if (sscanf(s, "%d:%d:%d/%d:%d:%d|%d", &ph, &pm, &ps, &dh, &dm, &ds, &pp) ==
      7) {
    *pos = ph * 3600 + pm * 60 + ps;
    *dur = dh * 3600 + dm * 60 + ds;
    *pct = pp;
    return;
  }
  if (sscanf(s, "%d:%d/%d:%d|%d", &pm, &ps, &dm, &ds, &pp) == 5) {
    *pos = pm * 60 + ps;
    *dur = dm * 60 + ds;
    *pct = pp;
  }
}

static int poll_one(Source *s, const char *bus) {
  if (strstr(bus, "plasma-browser-integration"))
    return 0;
  if (strstr(bus, "playerctld"))
    return 0;

  char cmd[512];

  snprintf(cmd, sizeof(cmd), SCRIPT_PATH " status '%s'", bus);
  char *st = run_line(cmd);
  if (!st || (strcmp(st, "Playing") && strcmp(st, "Paused"))) {
    free(st);
    return 0;
  }
  s->playing = !strcmp(st, "Playing");
  free(st);

  snprintf(cmd, sizeof(cmd), SCRIPT_PATH " title '%s'", bus);
  char *t = run_line(cmd);
  strncpy(s->title, t ? t : "unknown", 255);
  free(t);

  snprintf(cmd, sizeof(cmd), SCRIPT_PATH " artist '%s'", bus);
  char *a = run_line(cmd);
  if (a && strcmp(a, "—") && a[0])
    strncpy(s->artist, a, 255);
  else
    s->artist[0] = '\0';
  free(a);

  snprintf(cmd, sizeof(cmd), SCRIPT_PATH " player-name '%s'", bus);
  char *pn = run_line(cmd);
  strncpy(s->player_name, pn ? pn : "?", 63);
  free(pn);

  snprintf(cmd, sizeof(cmd), SCRIPT_PATH " progress '%s'", bus);
  char *prog = run_line(cmd);
  parse_progress(prog, &s->position_sec, &s->duration_sec, &s->percentage);
  free(prog);

  strncpy(s->bus, bus, 255);
  return 1;
}

static int src_cmp(const void *a, const void *b) {
  return ((Source *)b)->playing - ((Source *)a)->playing;
}

static void poll_all(MprisState *s) {
  int old_height = (s->n > 1) ? 116 : 56;
  s->n = 0;
  char *all = run_all(SCRIPT_PATH " list");
  if (!all)
    return;

  char *save = NULL;
  char *tok = strtok_r(all, "\n", &save);
  while (tok && s->n < MAX_SOURCES) {
    while (*tok == ' ' || *tok == '\t')
      tok++;
    if (*tok && poll_one(&s->src[s->n], tok))
      s->n++;
    tok = strtok_r(NULL, "\n", &save);
  }
  free(all);

  qsort(s->src, s->n, sizeof(Source), src_cmp);

  int maxoff = s->n > 2 ? s->n - 2 : 0;
  if (s->scroll_offset > maxoff)
    s->scroll_offset = maxoff;
  if (s->scroll_offset < 0)
    s->scroll_offset = 0;

  int new_height = (s->n > 1) ? 116 : 56;
  if (s->initialized && new_height != old_height) {
    s->need_relayout = 1;
  }
}

/* ── module callbacks ──────────────────────────── */
static void mpris_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  MprisState *s = calloc(1, sizeof(*s));
  m->priv = s;
  s->dragging = -1;
  s->initialized = 0;
  s->need_relayout = 0;
  poll_all(s);
}

static void mpris_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  MprisState *s = (MprisState *)m->priv;
  s->initialized = 1; /* safe to relayout after first draw */

  int cx = x + m->margin_left;
  int cy = y + m->margin_top;

  /* overall background (the whole module area) */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  /* optional: a thin border around the whole module – remove if you want only
   * box borders */
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  if (s->n == 0) {
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
  int box_h = 56;                      /* total height per box (2 lines) */
  int visible = (s->n > 2) ? 2 : s->n; /* at most 2 boxes */
  int start_i = s->scroll_offset;
  int total_visible_h = box_h * visible + BOX_GAP * (visible - 1);

  /* centre the visible boxes vertically inside the module */
  int start_y = y + (h - total_visible_h) / 2;
  if (start_y < y + pad)
    start_y = y + pad;

  int max_tw = (w * 9) / 10;

  for (int i = 0; i < visible; i++) {
    Source *src = &s->src[start_i + i];
    int box_top = start_y + i * (box_h + BOX_GAP); /* top of this box */

    /* draw the box border (a card for this source) */
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + pad, box_top, w - 2 * pad,
                   box_h - 1);

    /* reset hit areas for this box */
    src->src_x1 = src->src_x2 = 0;
    src->btn_x1 = src->btn_x2 = 0;
    src->trk_x1 = src->trk_x2 = 0;
    src->trk_y1 = src->trk_y2 = 0;

    /* inner drawing coordinates (leave space for border) */
    int inner_x = x + pad + 2;
    int inner_w = w - 2 * pad - 4;

    /* ── line 1: title – artist   player name ── */
    int line1_y = box_top + 2;
    char line1[512];
    if (src->artist[0])
      snprintf(line1, sizeof(line1), "%s – %s", src->title, src->artist);
    else
      snprintf(line1, sizeof(line1), "%s", src->title);
    int tw1 = drw_fontset_getwidth(drw, line1);
    const char *sname = src->player_name;
    int snw = drw_fontset_getwidth(drw, sname);
    int gap = 8;
    int avail = max_tw - snw - gap;
    if (tw1 > avail)
      tw1 = avail;

    drw_setscheme(drw, scheme[0]);
    drw_text(drw, inner_x, line1_y, tw1, line_h, 0, line1, 0);

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
    int snx = inner_x + tw1 + gap;
    int sny = line1_y + (line_h - font_h) / 2 + drw->fonts->xfont->ascent;
    XftDraw *xd = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xd, &sc, drw->fonts->xfont, snx, sny,
                      (const XftChar8 *)sname, strlen(sname));
    XftDrawDestroy(xd);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &sc);

    src->src_x1 = snx - cx;
    src->src_x2 = snx + snw - cx;
    src->src_y1 = box_top + 2 - cy;
    src->src_y2 = box_top + 2 + line_h - cy;

    /* ── line 2: play/pause button, time, progress bar ── */
    int bar_y = box_top + 2 + line_h; /* top of second line */
    int bar_pad = 8, bar_h = 10, knob = 10;
    int bar_center_y = bar_y + line_h / 2 - bar_h / 2;

    int btn_sz = line_h - 4;
    int bnx = inner_x + bar_pad;
    int bny = bar_y + (line_h - btn_sz) / 2;
    unsigned long accent = xpixel(COL_ACCENT);
    XSetForeground(dpy, drw->gc, accent);
    XFillRectangle(dpy, drw->drawable, drw->gc, bnx, bny, btn_sz, btn_sz);
    XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
    XDrawRectangle(dpy, drw->drawable, drw->gc, bnx, bny, btn_sz - 1,
                   btn_sz - 1);

    const char *icon = src->playing ? "||" : "|>";
    int iw = drw_fontset_getwidth(drw, icon);
    XftColor ifg;
    {
      XColor xc;
      XParseColor(dpy, DefaultColormap(dpy, screen), COL_BG, &xc);
      XAllocColor(dpy, DefaultColormap(dpy, screen), &xc);
      XRenderColor xrc = {xc.red, xc.green, xc.blue, 0xFFFF};
      XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), &xrc, &ifg);
    }
    XftDraw *xd2 = XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                                 DefaultColormap(dpy, screen));
    XftDrawStringUtf8(xd2, &ifg, drw->fonts->xfont, bnx + (btn_sz - iw) / 2,
                      bny + (btn_sz - font_h) / 2 + drw->fonts->xfont->ascent,
                      (const XftChar8 *)icon, strlen(icon));
    XftDrawDestroy(xd2);
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                 &ifg);

    src->btn_x1 = bnx - cx;
    src->btn_x2 = bnx + btn_sz - cx;
    src->btn_y1 = bny - cy;
    src->btn_y2 = bny + btn_sz - cy;

    /* time strings */
    int ts = bnx + btn_sz + 4;
    char tcur[16], ttot[16];
    snprintf(tcur, sizeof(tcur), "%d:%02d", src->position_sec / 60,
             src->position_sec % 60);
    snprintf(ttot, sizeof(ttot), "%d:%02d", src->duration_sec / 60,
             src->duration_sec % 60);
    int tw_cur = drw_fontset_getwidth(drw, tcur);
    drw_setscheme(drw, scheme[0]);
    drw_text(drw, ts, bar_y, tw_cur, line_h, 0, tcur, 0);
    ts += tw_cur + 4;

    int tw_tot = drw_fontset_getwidth(drw, ttot);
    int total_x = inner_x + inner_w - bar_pad - tw_tot;
    drw_text(drw, total_x, bar_y, tw_tot, line_h, 0, ttot, 0);

    int trk_end = total_x - 4;
    int trk_len = trk_end - ts;
    if (trk_len < 16)
      trk_len = 16;
    float frac = src->percentage / 100.0f;
    int kx = ts + (int)(frac * trk_len) - knob / 2;
    if (kx < ts)
      kx = ts;
    if (kx + knob > trk_end)
      kx = trk_end - knob;

    XSetForeground(dpy, drw->gc, xpixel(COL_GREEN));
    XFillRectangle(dpy, drw->drawable, drw->gc, ts, bar_center_y, kx - ts,
                   bar_h);
    XSetForeground(dpy, drw->gc, 0x3b4252);
    XFillRectangle(dpy, drw->drawable, drw->gc, kx + knob, bar_center_y,
                   trk_end - (kx + knob), bar_h);

    int ky = bar_y + (line_h - knob) / 2;
    XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, kx, ky, knob, knob);

    src->trk_x1 = ts - cx;
    src->trk_x2 = trk_end - cx;
    src->trk_y1 = bar_y - cy;
    src->trk_y2 = bar_y + line_h - cy;
  }
}

/* ── input handler ───────────────────────────────── */
static void mpris_input(Module *m, const InputEvent *ev) {
  MprisState *s = (MprisState *)m->priv;

  /* scroll – works anywhere in the module */
  if (ev->type == EV_SCROLL) {
    int maxoff = s->n > 2 ? s->n - 2 : 0;
    int new_off = s->scroll_offset + ev->scroll_dy;
    if (new_off < 0)
      new_off = 0;
    if (new_off > maxoff)
      new_off = maxoff;
    if (new_off != s->scroll_offset) {
      s->scroll_offset = new_off;
      panel_redraw();
    }
    return;
  }

  if (s->n == 0)
    return;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    int start_i = s->scroll_offset;
    int visible = s->n > 2 ? 2 : s->n;

    for (int i = 0; i < visible; i++) {
      Source *src = &s->src[start_i + i];

      if (ev->x >= src->btn_x1 && ev->x < src->btn_x2 && ev->y >= src->btn_y1 &&
          ev->y < src->btn_y2) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), SCRIPT_PATH " play-pause '%s'", src->bus);
        system(cmd);
        poll_all(s);
        panel_redraw();
        return;
      }

      if (ev->x >= src->trk_x1 && ev->x <= src->trk_x2 &&
          ev->y >= src->trk_y1 && ev->y < src->trk_y2) {
        s->dragging = start_i + i;
        s->drag_start_x = ev->x;
        s->drag_start_pct = src->percentage;
        return;
      }

      if (ev->x >= src->src_x1 && ev->x < src->src_x2 && ev->y >= src->src_y1 &&
          ev->y < src->src_y2) {
        if (strcmp(src->player_name, "firefox") == 0)
          system("firefox --new-window about:blank &");
        else if (strcmp(src->player_name, "vlc") == 0)
          system("vlc &");
        return;
      }
    }
  }

  if (ev->type == EV_MOTION && s->dragging >= 0) {
    Source *src = &s->src[s->dragging];
    int len = src->trk_x2 - src->trk_x1;
    if (len < 1)
      len = 1;

    int clamped_x = ev->x;
    if (clamped_x < src->trk_x1)
      clamped_x = src->trk_x1;
    if (clamped_x > src->trk_x2)
      clamped_x = src->trk_x2;

    int new_pct = (clamped_x - src->trk_x1) * 100 / len;
    if (new_pct < 0)
      new_pct = 0;
    if (new_pct > 100)
      new_pct = 100;

    int diff = new_pct - s->drag_start_pct;
    if (diff) {
      char cmd[512];
      snprintf(cmd, sizeof(cmd), SCRIPT_PATH " seek '%s' %+d%%", src->bus,
               diff);
      system(cmd);
      s->drag_start_pct = new_pct;
      src->percentage = new_pct;
      panel_redraw();
    }
    return;
  }

  if (ev->type == EV_RELEASE && ev->button == Button1 && s->dragging >= 0) {
    s->dragging = -1;
    poll_all(s);
    panel_redraw();
  }
}

/* ── timer (2 sec) ───────────────────────────────── */
static void mpris_timer(Module *m) {
  MprisState *s = (MprisState *)m->priv;
  time_t now = time(NULL);
  if (now - s->last_poll >= 2) {
    s->last_poll = now;
    if (s->dragging < 0)
      poll_all(s);
    if (s->need_relayout) {
      s->need_relayout = 0;
      panel_relayout();
      panel_redraw();
    }
    panel_redraw();
  }
}

static void mpris_destroy(Module *m) { free(m->priv); }

/* ── hints: 56 for 0/1 source, 116 for 2+ ────────── */
static LayoutHints *mpris_hints(Module *m) {
  static LayoutHints hints;
  MprisState *s = (MprisState *)m->priv;
  int height = (s && s->n > 1) ? 116 : 56;
  hints.min_h = hints.pref_h = hints.max_h = height;
  hints.expand_y = 0;
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
