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
static int top_h = 0;
static int top_expanded = 0;
static int top_hovered = 0;
static int drag_start_y = 0;
static int dragging = 0;
static int drag_threshold = 20;

static void topbar_draw(void);
static void toggle_panel(void);

/* read _MUHH_PANEL_VISIBLE atom – no side effects */
static int is_panel_visible(void) {
  Atom a = XInternAtom(wm.dpy, "_MUHH_PANEL_VISIBLE", False);
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *data = NULL;
  int visible = 0;
  if (XGetWindowProperty(wm.dpy, wm.root, a, 0, 1, False, XA_CARDINAL, &type,
                         &format, &n, &after, &data) == Success &&
      data) {
    visible = *(unsigned long *)data != 0;
    XFree(data);
  }
  return visible;
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

static void topbar_draw(void) {
  if (top_win == None)
    return;
  unsigned int w = (unsigned int)wm.sw;
  unsigned int h = (unsigned int)top_h;

  int bg = top_expanded ? top_edge_expanded_bg : top_edge_bg;
  XSetForeground(wm.dpy, wm.drw->gc, bg);
  XFillRectangle(wm.dpy, wm.drw->drawable, wm.drw->gc, 0, 0, w, h);

  if (top_expanded) {
    const char *island_text = "island";
    int tw = drw_fontset_getwidth(wm.drw, island_text);
    int island_w = tw + 20;
    int island_h = h - 6;
    int island_x = (w - island_w) / 2;
    int island_y = 3;

    XSetForeground(wm.dpy, wm.drw->gc, island_bg);
    XFillRectangle(wm.dpy, wm.drw->drawable, wm.drw->gc, island_x, island_y,
                   island_w, island_h);

    XSetForeground(wm.dpy, wm.drw->gc, top_edge_text_color);
    drw_text(wm.drw, island_x + 10,
             island_y + (island_h - wm.drw->fonts->h) / 2, tw, wm.drw->fonts->h,
             0, island_text, 0);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[6];
    strftime(time_str, sizeof(time_str), "%H:%M", tm);
    int tw_clock = drw_fontset_getwidth(wm.drw, time_str);
    int clock_y = (h - wm.drw->fonts->h) / 2;
    XSetForeground(wm.dpy, wm.drw->gc, top_edge_text_color);
    drw_text(wm.drw, 10, clock_y, tw_clock, wm.drw->fonts->h, 0, time_str, 0);
  }

  drw_map(wm.drw, top_win, 0, 0, w, h);
}

void topbar_init(void) {
  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = top_edge_bg;
  wa.event_mask = EnterWindowMask | LeaveWindowMask | ButtonPressMask |
                  ButtonReleaseMask | PointerMotionMask | Button4MotionMask |
                  Button5MotionMask | ExposureMask;
  top_h = top_edge_zone;
  top_win = XCreateWindow(wm.dpy, wm.root, 0, 0, (unsigned int)wm.sw,
                          (unsigned int)top_h, 0, CopyFromParent, InputOutput,
                          CopyFromParent,
                          CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
  XMapRaised(wm.dpy, top_win);

  /* force scroll‑down and scroll‑up events to reach us */
  XGrabButton(wm.dpy, Button4, AnyModifier, top_win, True, ButtonPressMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(wm.dpy, Button5, AnyModifier, top_win, True, ButtonPressMask,
              GrabModeAsync, GrabModeAsync, None, None);

  topbar_draw(); /* never hidden */
}

int topbar_handle_event(XEvent *e) {
  if (e->xany.window != top_win)
    return 0;

  switch (e->type) {
  case EnterNotify:
    top_hovered = 1;
    if (!top_expanded) {
      top_expanded = 1;
      top_h = top_edge_expanded;
      XResizeWindow(wm.dpy, top_win, (unsigned int)wm.sw, (unsigned int)top_h);
      topbar_draw();
    }
    break;
  case LeaveNotify:
    top_hovered = 0;
    if (top_expanded && !dragging) {
      top_expanded = 0;
      top_h = top_edge_zone;
      XResizeWindow(wm.dpy, top_win, (unsigned int)wm.sw, (unsigned int)top_h);
      topbar_draw();
    }
    break;
  case ButtonPress:
    if (e->xbutton.button == Button4) { /* scroll‑up */
      if (is_panel_visible())
        toggle_panel();                        /* close panel */
    } else if (e->xbutton.button == Button5) { /* scroll‑down */
      if (!is_panel_visible())
        toggle_panel();                        /* open panel */
    } else if (e->xbutton.button == Button1) { /* left click drag */
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
        toggle_panel(); /* drag down opens / hides */
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

void topbar_update_visibility(void) {
  int panel_open = is_panel_visible();

  if (panel_open) {
    /* Panel is visible – hide the trigger bar completely */
    XUnmapWindow(wm.dpy, top_win);
  } else {
    /* Panel closed – show the trigger bar again (thin, idle state) */
    XMapRaised(wm.dpy, top_win);
    if (top_expanded) {
      top_expanded = 0;
      top_h = top_edge_zone;
      XResizeWindow(wm.dpy, top_win, (unsigned int)wm.sw, (unsigned int)top_h);
    }
    topbar_draw();
  }
}
