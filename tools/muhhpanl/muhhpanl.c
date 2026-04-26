#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "common/panel_globals.h"
#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "settings.h"
#include "timeline/timeline.h"
#include "util.h"

/* ── globals (shared with modules) ──────────────── */
Display *dpy;
Window root;
Drw *drw;
Clr **scheme;
int panel_w, panel_h;

/* ── private state ──────────────────────────────── */
int screen;
Window panel_win;
static int sw, sh, barh;
int panel_x, panel_y;
int panel_shown = 0;
static int animating = 0, anim_step = 0, anim_dir = 0;
static int dirty = 1;

static Module *root_container;

/* ── forward declarations ───────────────────────── */
static void init_scheme(void);
static int get_bar_height(void);
static void update_geometry(void);
static void move_panel(int y);
static void show_panel(void);
static void hide_panel(void);
static void animation_tick(void);
static void draw_panel(void);
static void build_layout(void);
static void event_loop(void);
void mpvbox_register(void);
void mpvsearch_register(void);
void profanity_register(void);

/* mpvbox globals – defined in modules/bottom/mpvbox.c */
extern int mpvbox_x, mpvbox_y, mpvbox_w, mpvbox_h;
extern Window mpv_win_export;
extern void mpvbox_show(void);
extern void mpvbox_hide(void);

/* profanity globals – defined in modules/bottom/profanity.c */
extern int profanity_x, profanity_y, profanity_w, profanity_h;
extern Window profanity_win_export;
void profanity_show(void);
void profanity_hide(void);

void panel_redraw(void) { dirty = 1; }
void panel_hide(void) {
  if (panel_shown)
    hide_panel();
}

/* ── colour scheme ──────────────────────────────── */
static void init_scheme(void) {
  scheme = ecalloc(LENGTH(panel_colors), sizeof(Clr *));
  for (size_t i = 0; i < LENGTH(panel_colors); i++)
    scheme[i] = drw_scm_create(drw, panel_colors[i], 3);
}

/* ── bar height (from muhhwm) ───────────────────── */
static int get_bar_height(void) {
  Atom a = XInternAtom(dpy, "_MUHH_BAR_HEIGHT", False);
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *data = NULL;
  int h = 0;
  if (XGetWindowProperty(dpy, root, a, 0, 1, False, XA_CARDINAL, &type, &format,
                         &n, &after, &data) == Success &&
      data) {
    h = (int)*(unsigned long *)data;
    XFree(data);
  }
  return h;
}

/* ── geometry ───────────────────────────────────── */
static void update_geometry(void) {
  barh = get_bar_height();
  panel_w = (sw * panel_width_pct) / 100;
  panel_x = panel_left_gap;
  if (panel_x + panel_w > sw)
    panel_x = sw - panel_w;
  if (panel_x < 0)
    panel_x = 0;
  panel_h = (sh * panel_height_pct) / 100;
  if (panel_h > sh - barh)
    panel_h = sh - barh;
}

static void move_panel(int y) {
  XMoveWindow(dpy, panel_win, panel_x, y);
  XFlush(dpy);
}

/* ── show / hide ────────────────────────────────── */
static void show_panel(void) {
  if (panel_shown)
    return;
  panel_shown = 1;
  XMapWindow(dpy, panel_win);
  XRaiseWindow(dpy, panel_win);
  XSetInputFocus(dpy, panel_win, RevertToPointerRoot, CurrentTime);

  /* notify muhhwm */
  Atom visible = XInternAtom(dpy, "_MUHH_PANEL_VISIBLE", False);
  unsigned long data = 1;
  XChangeProperty(dpy, root, visible, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&data, 1);

  XGrabButton(dpy, AnyButton, AnyModifier, root, True, ButtonPressMask,
              GrabModeAsync, GrabModeAsync, None, None);
  animating = 1;
  anim_step = 0;
  anim_dir = 1;
  panel_y = -panel_h;
  move_panel(panel_y);
  mpvbox_show();
  profanity_show();
}

static void hide_panel(void) {
  if (!panel_shown)
    return;
  panel_shown = 0;

  Atom visible = XInternAtom(dpy, "_MUHH_PANEL_VISIBLE", False);
  unsigned long data = 0;
  XChangeProperty(dpy, root, visible, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&data, 1);

  XUngrabButton(dpy, AnyButton, AnyModifier, root);
  animating = 1;
  anim_step = 0;
  anim_dir = -1;
  mpvbox_hide();
  profanity_hide();
}

/* ── animation ──────────────────────────────────── */
static void animation_tick(void) {
  if (!animating)
    return;
  anim_step++;
  float p = (float)anim_step / SHOW_STEPS;
  if (p > 1.0f)
    p = 1.0f;
  int target = anim_dir > 0 ? 0 : -panel_h;
  panel_y = target + (int)((anim_dir > 0 ? -panel_h : panel_h) * (1.0f - p));
  move_panel(panel_y);
  if (anim_step >= SHOW_STEPS) {
    animating = 0;
    if (anim_dir < 0) {
      XUnmapWindow(dpy, panel_win);
      panel_y = -panel_h;
      move_panel(panel_y);
    }
  }
  dirty = 1;
}

/* ── drawing ────────────────────────────────────── */
static void draw_panel(void) {
  if (!panel_shown && !animating)
    return;
  drw_setscheme(drw, scheme[0]);
  drw_rect(drw, 0, 0, panel_w, panel_h, 1, 1);
  if (root_container && root_container->draw)
    root_container->draw(root_container, 0, 0, panel_w, panel_h, 0);
  drw_map(drw, panel_win, 0, 0, panel_w, panel_h);
  dirty = 0;
}

/* ── client message handler ─────────────────────── */
static void handle_client_message(XClientMessageEvent *ev) {
  Atom toggle = XInternAtom(dpy, "_MUHH_PANEL_TOGGLE", False);
  if (ev->message_type == toggle) {
    if (panel_shown)
      hide_panel();
    else
      show_panel();
  }
}

/* ── layout builder (declarative tree) ──────────── */
static void build_layout(void) {
  extern LayoutNode layout_tree;
  root_container = container_build_tree(&layout_tree);
  if (!root_container) {
    fprintf(stderr, "muhhpanl: failed to build layout tree\n");
    exit(1);
  }
  container_layout(root_container, 0, 0, panel_w, panel_h);
}

/* ── event loop ─────────────────────────────────── */
static void event_loop(void) {
  XEvent ev;
  int xfd = ConnectionNumber(dpy);
  struct timespec anim_ts = {0};

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(xfd, &fds);

    struct timeval tv;
    if (animating) {
      long elapsed = 0;
      if (anim_step > 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - anim_ts.tv_sec) * 1000 +
                  (now.tv_nsec - anim_ts.tv_nsec) / 1000000;
      }
      long wait = SHOW_STEP_DELAY_MS - elapsed;
      if (wait < 0)
        wait = 0;
      tv.tv_sec = wait / 1000;
      tv.tv_usec = (wait % 1000) * 1000;
    } else {
      tv.tv_sec = 0;
      tv.tv_usec = 500000;
    }

    int ret = select(xfd + 1, &fds, NULL, NULL, &tv);
    if (ret < 0)
      continue;

    if (animating && (anim_step == 0 || ret == 0)) {
      animation_tick();
      clock_gettime(CLOCK_MONOTONIC, &anim_ts);
    }

    /* 1‑second timer */
    {
      static time_t last_timer = 0;
      time_t now = time(NULL);
      if (now - last_timer >= 1) {
        if (root_container && root_container->timer)
          root_container->timer(root_container);
        last_timer = now;
      }
    }

    while (XPending(dpy)) {
      XNextEvent(dpy, &ev);

      /* ClientMessage */
      if (ev.type == ClientMessage) {
        handle_client_message(&ev.xclient);
        continue;
      }

      /* Translate and dispatch */
      InputEvent iev;
      if (translate_event(&ev, &iev)) {
        if (!panel_shown)
          continue;

        /* ── mpv event replay (mouse + keyboard) ── */
        if (mpvbox_w > 0 && iev.root_x >= mpvbox_x &&
            iev.root_x < mpvbox_x + mpvbox_w && iev.root_y >= mpvbox_y &&
            iev.root_y < mpvbox_y + mpvbox_h) {
          if (iev.type == EV_PRESS && mpv_win_export) {
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            continue;
          }
          if (iev.type == EV_KEY_PRESS && mpv_win_export) {
            XSendEvent(dpy, mpv_win_export, True, KeyPressMask, &ev);
            continue;
          }
        }

        /* ── profanity event replay (mouse + keyboard) ── */
        if (profanity_w > 0 && iev.root_x >= profanity_x &&
            iev.root_x < profanity_x + profanity_w &&
            iev.root_y >= profanity_y &&
            iev.root_y < profanity_y + profanity_h) {
          if (iev.type == EV_PRESS && profanity_win_export) {
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            continue;
          }
        }

        /* hide panel on outside click (buttons 1‑3) */
        if (iev.type == EV_PRESS && iev.button >= 1 && iev.button <= 3 &&
            (iev.root_x < panel_x || iev.root_x >= panel_x + panel_w ||
             iev.root_y < 0 || iev.root_y >= panel_h)) {
          hide_panel();
          continue;
        }

        /* Escape key hides the panel */
        if (iev.type == EV_KEY_PRESS &&
            XLookupKeysym(&ev.xkey, 0) == XK_Escape) {
          hide_panel();
          continue;
        }

        if (root_container && root_container->input)
          root_container->input(root_container, &iev);
      }

      /* Expose */
      if (ev.type == Expose && ev.xexpose.count == 0)
        dirty = 1;
    }

    if (dirty)
      draw_panel();
  }
}

/* ── main ───────────────────────────────────────── */
int main(void) {
  dpy = XOpenDisplay(NULL);
  if (!dpy)
    die("muhhpanl: cannot open display\n");
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);

  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, panel_fonts, LENGTH(panel_fonts)))
    die("muhhpanl: no fonts loaded\n");
  init_scheme();
  init_themes();
  update_geometry();

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = scheme[0][1].pixel;
  wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                  KeyPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask;
  panel_win =
      XCreateWindow(dpy, root, panel_x, -panel_h, panel_w, panel_h, 0,
                    CopyFromParent, InputOutput, CopyFromParent,
                    CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
  XClassHint ch = {"muhhpanl", "muhhpanl"};
  XSetClassHint(dpy, panel_win, &ch);

  Atom wid_atom = XInternAtom(dpy, "_MUHH_PANEL_WID", False);
  XChangeProperty(dpy, root, wid_atom, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&panel_win, 1);
  XFlush(dpy);

  /* register modules that aren't auto‑registered */
  timeline_register();
  mpvbox_register();
  mpvsearch_register();
  profanity_register();
  build_layout();
  event_loop();
  return 0;
}
