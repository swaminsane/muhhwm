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

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "modules/topstrip/topstrip.h"
#include "panel.h"
#include "settings.h"
#include "timeline/timeline.h"
#include "util.h"

Display *dpy;
Window root;
Drw *drw;
Clr **scheme;
int panel_w, panel_h;

static int screen;
static Window panel_win;
static int sw, sh, barh;
static int panel_x, panel_y;
static int panel_shown = 0;
static int animating = 0, anim_step = 0, anim_dir = 0;
static int dirty = 1;

Module *root_container;

/* ── helper prototypes ─────────────────────────── */
static void init_scheme(void);
static int get_bar_height(void);
static void update_geometry(void);
static void move_panel(int y);
void panel_redraw(void);
static void show_panel(void);
void hide_panel(void);
static void animation_tick(void);
static void draw_panel(void);
static void handle_client_message(XClientMessageEvent *ev);
static void build_layout(void);
static void event_loop(void);

void panel_hide(void) {
  if (panel_shown)
    hide_panel();
}

/* ── color scheme ─────────────────────────────── */
static void init_scheme(void) {
  scheme = ecalloc(LENGTH(panel_colors), sizeof(Clr *));
  for (size_t i = 0; i < LENGTH(panel_colors); i++)
    scheme[i] = drw_scm_create(drw, panel_colors[i], 3);
}

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

void panel_redraw(void) { dirty = 1; }

static void show_panel(void) {
  if (panel_shown)
    return;
  panel_shown = 1;
  XMapWindow(dpy, panel_win);
  XRaiseWindow(dpy, panel_win);
  XSetInputFocus(dpy, panel_win, RevertToPointerRoot, CurrentTime);

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
}

void hide_panel(void) {
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
}

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

static void handle_client_message(XClientMessageEvent *ev) {
  Atom toggle = XInternAtom(dpy, "_MUHH_PANEL_TOGGLE", False);
  if (ev->message_type == toggle) {
    if (panel_shown)
      hide_panel();
    else
      show_panel();
  }
}

/* ── build full layout (now using container_layout) ─────── */
static void build_layout(void) {
  int fixed_sum = 0;
  int pct_sum = 0;
  for (size_t r = 0; r < NUM_ROWS; r++) {
    int hpct = layout_rows[r].height_pct;
    if (hpct < 0)
      fixed_sum += -hpct;
    else
      pct_sum += hpct;
  }
  int remaining_h = panel_h - fixed_sum;
  if (remaining_h < 0)
    remaining_h = 0;

  Module *root_vert = container_create_manual(1);

  for (size_t r = 0; r < NUM_ROWS; r++) {
    int hpct = layout_rows[r].height_pct;
    int row_h = (hpct < 0) ? -hpct : (remaining_h * hpct) / pct_sum;

    Module *row_horiz = container_create_manual(0);
    row_horiz->h = row_h;

    for (int c = 0; c < 4 && layout_rows[r].col_widths[c] > 0; c++) {
      int col_w = (panel_w * layout_rows[r].col_widths[c]) / 100;
      const char **modlist = layout_rows[r].col_modules[c];
      Module *col = container_create(modlist, 1);
      col->w = col_w;
      container_add_child(row_horiz, col);
    }
    container_add_child(root_vert, row_horiz);
  }
  root_container = root_vert;
  container_layout(root_container, 0, 0, panel_w, panel_h);
}

/* ── event loop ────────────────────────────────── */
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

    /* coarse 1‑second timer */
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
      switch (ev.type) {
      case ClientMessage:
        handle_client_message(&ev.xclient);
        break;
      case Expose:
        if (ev.xexpose.count == 0)
          dirty = 1;
        break;

      case ButtonPress:
        if (!panel_shown)
          break;

        /* scroll‑wheel / two‑finger scroll */
        if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5) {
          int dir = (ev.xbutton.button == Button4) ? 1 : -1;
          if (root_container && root_container->scroll)
            root_container->scroll(root_container, ev.xbutton.x, ev.xbutton.y,
                                   dir);
          break;
        }

        /* outside click (buttons 1‑3) → hide panel */
        if ((ev.xbutton.button == Button1 || ev.xbutton.button == Button2 ||
             ev.xbutton.button == Button3) &&
            (ev.xbutton.x_root < panel_x ||
             ev.xbutton.x_root >= panel_x + panel_w || ev.xbutton.y_root < 0 ||
             ev.xbutton.y_root >= panel_h)) {
          hide_panel();
          break;
        }

        /* inside panel: forward to modules */
        if (ev.xbutton.window == panel_win && root_container &&
            root_container->click)
          root_container->click(root_container, ev.xbutton.x, ev.xbutton.y,
                                ev.xbutton.button);
        break;

      case MotionNotify:
        if (!panel_shown)
          break;
        if (ev.xmotion.window == panel_win && root_container &&
            root_container->motion)
          root_container->motion(root_container, ev.xmotion.x, ev.xmotion.y);
        break;

      case LeaveNotify:
        if (!panel_shown)
          break;
        /* signal all modules that the pointer left the panel */
        if (ev.xcrossing.window == panel_win && root_container &&
            root_container->motion)
          root_container->motion(root_container, -1, -1);
        break;

      case KeyPress:
        if (!panel_shown)
          break;
        {
          KeySym ks = XLookupKeysym(&ev.xkey, 0);
          if (ks == XK_Escape) {
            hide_panel();
            break;
          }
          /* future: forward other keys to modules via a 'key' callback */
        }
        break;
      }
    }

    if (dirty)
      draw_panel();
  }
}

int main(void) {
  dpy = XOpenDisplay(NULL);
  if (!dpy)
    die("muhhpanel: cannot open display\n");
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);

  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, panel_fonts, LENGTH(panel_fonts)))
    die("muhhpanel: no fonts loaded\n");
  init_scheme();
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
  XClassHint ch = {"muhhpanel", "muhhpanel"};
  XSetClassHint(dpy, panel_win, &ch);

  Atom wid_atom = XInternAtom(dpy, "_MUHH_PANEL_WID", False);
  XChangeProperty(dpy, root, wid_atom, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&panel_win, 1);
  XFlush(dpy);

  timeline_register();
  topstrip_register();
  build_layout();
  event_loop();
  return 0;
}
