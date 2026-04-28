#include "topbar.h"
#include "config.h"
#include "muhh.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Window top_win = None;

/* ── geometry ──────────────────────────────────── */
static int idle_h = 4;   /* collapsed height */
static int expand_h = 0; /* expanded height (set in init) */
static int cur_h = 4;    /* current height */

static int expanded = 0; /* 1 = mouse hovering */
static int dragging = 0;
static int drag_start_y = 0;
static int drag_threshold = 20;
static int visible = 1; /* 1 = on screen, 0 = hidden by panel */

/* ── cached screen‑time string ─────────────────── */
static char screen_time_str[64] = "";
static time_t last_screen_time = 0;

/* ── helpers ────────────────────────────────────── */
static void topbar_draw(void);
static void toggle_panel(void);

static int is_panel_visible(void) {
  Atom a = XInternAtom(wm.dpy, "_MUHH_PANEL_VISIBLE", False);
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *data = NULL;
  int vis = 0;
  if (XGetWindowProperty(wm.dpy, wm.root, a, 0, 1, False, XA_CARDINAL, &type,
                         &format, &n, &after, &data) == Success &&
      data) {
    vis = *(unsigned long *)data != 0;
    XFree(data);
  }
  return vis;
}

static void toggle_panel(void) {
  Atom wid_atom = XInternAtom(wm.dpy, "_MUHH_PANEL_WID", False);
  Atom toggle = XInternAtom(wm.dpy, "_MUHH_PANEL_TOGGLE", False);
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *data = NULL;
  Window panel_win = None;
  if (XGetWindowProperty(wm.dpy, wm.root, wid_atom, 0, 1, False, XA_WINDOW,
                         &type, &format, &n, &after, &data) == Success &&
      data) {
    panel_win = *(Window *)data;
    XFree(data);
  }
  if (!panel_win)
    return;
  XEvent ev = {0};
  ev.type = ClientMessage;
  ev.xclient.window = panel_win;
  ev.xclient.message_type = toggle;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1;
  XSendEvent(wm.dpy, panel_win, False, NoEventMask, &ev);
  XFlush(wm.dpy);
}

/* ── battery reader ─────────────────────────────── */
static void read_battery(int *pct, int *charging) {
  *pct = 100;
  *charging = 0;
  FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
  if (f) {
    fscanf(f, "%d", pct);
    fclose(f);
  }
  f = fopen("/sys/class/power_supply/BAT0/status", "r");
  if (f) {
    char s[16] = {0};
    fgets(s, sizeof(s), f);
    fclose(f);
    *charging = (strncmp(s, "Charging", 8) == 0);
  }
}

/* ── screen time reader ─────────────────────────── */
static int read_total_screen_time(void) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  const char *home = getenv("HOME");
  if (!home)
    return 0;
  char logpath[512];
  snprintf(logpath, sizeof(logpath),
           "%s/.local/share/muhhwm/activity/%04d-%02d-%02d.log", home,
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
  FILE *f = fopen(logpath, "r");
  if (!f)
    return 0;
  long long total_ms = 0;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    long long ts, dur;
    if (sscanf(line, "%lld %lld", &ts, &dur) == 2)
      total_ms += dur;
  }
  fclose(f);
  return (int)(total_ms / 60000); /* minutes */
}

/* ── update screen‑time string ──────────────────── */
static void update_screen_time(void) {
  int mins = read_total_screen_time();
  snprintf(screen_time_str, sizeof(screen_time_str), "screen time %dh%02dm",
           mins / 60, mins % 60);
}

/* ── draw the bar ───────────────────────────────── */
static void topbar_draw(void) {
  if (top_win == None || !visible)
    return;

  int w = wm.sw, h = cur_h;
  drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
  drw_rect(wm.drw, 0, 0, w, h, 1, 1); /* background fill */

  if (!expanded) {
    drw_map(wm.drw, top_win, 0, 0, w, h);
    return; /* idle: just a thin line */
  }

  int font_h = wm.drw->fonts->h;
  int pad = 6;

  /* ── left: screen time + battery ── */
  int bat_pct, charging;
  read_battery(&bat_pct, &charging);

  /* screen time string (already updated in tick) */
  char st_str[64];
  int mins = read_total_screen_time();
  snprintf(st_str, sizeof(st_str), "screen time %dh%02dm ", mins / 60,
           mins % 60);

  /* draw screen time in normal scheme */
  drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
  drw_text(wm.drw, pad, 0, TEXTW(st_str), h, 0, st_str, 0);

  /* battery percentage */
  char bstr[16];
  if (charging)
    snprintf(bstr, sizeof(bstr), "%d%% *", bat_pct);
  else
    snprintf(bstr, sizeof(bstr), "%d%%", bat_pct);

  int bscheme = SchemeNorm;
  if (charging)
    bscheme = SchemeSel; /* green accent */
  else if (bat_pct < 15)
    bscheme = SchemeUrg; /* red */
  else if (bat_pct < 30)
    bscheme = SchemeNorm; /* orange accent not defined, normal for now */

  drw_setscheme(wm.drw, wm.scheme[bscheme]);
  drw_text(wm.drw, pad + TEXTW(st_str), 0, TEXTW(bstr), h, 0, bstr, 0);

  /* ── centre: clock ── */
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char clock_str[16];
  strftime(clock_str, sizeof(clock_str), "%H:%M:%S", tm);
  int cw = TEXTW(clock_str);
  drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
  drw_text(wm.drw, (w - cw) / 2, 0, cw, h, 0, clock_str, 0);

  /* ── right: now‑playing ── */
  Atom a = XInternAtom(wm.dpy, "_MUHH_MPV_TITLE", False);
  Atom type;
  int format;
  unsigned long nitems, after;
  unsigned char *data = NULL;
  char np_str[128] = "";
  if (XGetWindowProperty(wm.dpy, wm.root, a, 0, 100, False, XA_STRING, &type,
                         &format, &nitems, &after, &data) == Success &&
      data) {
    snprintf(np_str, sizeof(np_str), "Playing: %s", (char *)data);
    XFree(data);
  }

  if (np_str[0]) {
    int nw = TEXTW(np_str);
    int max_w = w / 3;
    while (nw > max_w && strlen(np_str) > 4) {
      np_str[strlen(np_str) - 1] = '\0';
      nw = TEXTW(np_str);
    }
    drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
    drw_text(wm.drw, w - nw - pad, 0, nw, h, 0, np_str, 0);
  }

  drw_map(wm.drw, top_win, 0, 0, w, h);
}

/* ── init ───────────────────────────────────────── */
void topbar_init(void) {
  expand_h = wm.drw->fonts->h + 4; /* expanded height based on font */
  cur_h = idle_h;

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = wm.scheme[SchemeNorm][ColBg].pixel;
  wa.event_mask = EnterWindowMask | LeaveWindowMask | ButtonPressMask |
                  Button4MotionMask | Button5MotionMask | PointerMotionMask |
                  ButtonReleaseMask | ExposureMask;
  top_win = XCreateWindow(wm.dpy, wm.root, 0, 0, wm.sw, cur_h, 0,
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
  XMapRaised(wm.dpy, top_win);
  update_screen_time();
  topbar_draw();
}

/* ── event handling ────────────────────────────── */
int topbar_handle_event(XEvent *e) {
  if (e->xany.window != top_win || !visible)
    return 0;

  switch (e->type) {
  case EnterNotify:
    if (!expanded) {
      expanded = 1;
      cur_h = expand_h;
      XResizeWindow(wm.dpy, top_win, wm.sw, cur_h);
      topbar_draw();
    }
    break;
  case LeaveNotify:
    if (expanded && !dragging) {
      expanded = 0;
      cur_h = idle_h;
      XResizeWindow(wm.dpy, top_win, wm.sw, cur_h);
      topbar_draw();
    }
    break;
  case ButtonPress:
    if (e->xbutton.button == Button4) { /* scroll up → close */
      if (is_panel_visible())
        toggle_panel();
    } else if (e->xbutton.button == Button5) { /* scroll down → open */
      if (!is_panel_visible())
        toggle_panel();
    } else if (e->xbutton.button == Button1) { /* drag start */
      drag_start_y = e->xbutton.y_root;
      dragging = 1;
      XGrabPointer(wm.dpy, top_win, False,
                   PointerMotionMask | ButtonReleaseMask, GrabModeAsync,
                   GrabModeAsync, None, None, CurrentTime);
    }
    break;
  case MotionNotify:
    if (dragging) {
      int dy = e->xmotion.y_root - drag_start_y;
      if (dy > drag_threshold) {
        toggle_panel();
        dragging = 0;
        XUngrabPointer(wm.dpy, CurrentTime);
      }
    }
    break;
  case ButtonRelease:
    if (e->xbutton.button == Button1 && dragging) {
      dragging = 0;
      XUngrabPointer(wm.dpy, CurrentTime);
    }
    break;
  case Expose:
    if (e->xexpose.count == 0)
      topbar_draw();
    break;
  }
  return 1;
}

/* ── visibility toggle (called when panel toggles) ── */
void topbar_update_visibility(void) {
  int panel_open = is_panel_visible();
  if (panel_open && visible) {
    XUnmapWindow(wm.dpy, top_win);
    visible = 0;
  } else if (!panel_open && !visible) {
    XMapRaised(wm.dpy, top_win);
    visible = 1;
    topbar_draw();
  }
}

/* ── periodic tick ──────────────────────────────── */
void topbar_tick(void) {
  if (!visible)
    return;
  static time_t last_clock = 0;
  time_t now = time(NULL);
  if (now != last_clock) { /* every second */
    last_clock = now;
    if (now - last_screen_time >= 30) {
      update_screen_time();
      last_screen_time = now;
    }
    topbar_draw();
  }
}
