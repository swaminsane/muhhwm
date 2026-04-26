#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"

/* exported screen coordinates for event replay */
int profanity_x, profanity_y, profanity_w, profanity_h;
Window profanity_win_export = None;

static pid_t prof_pid = 0;
static Window prof_win = None; /* our container */
static Window term_win = None; /* st’s internal window */
static int terminal_live = 0;

static int dummy_handler(Display *d, XErrorEvent *e) {
  (void)d;
  (void)e;
  return 0;
}

static void kill_profanity(void) {
  if (prof_pid > 0) {
    kill(prof_pid, SIGTERM);
    waitpid(prof_pid, NULL, 0);
    prof_pid = 0;
  }
  if (prof_win) {
    XDestroyWindow(dpy, prof_win);
    prof_win = None;
    profanity_win_export = None;
    term_win = None;
  }
  terminal_live = 0;
}

static void profanity_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
  terminal_live = 0;
  m->margin_top = 0;
  m->margin_bottom = 0;
  m->margin_left = 0;
  m->margin_right = 0;
}

static void profanity_draw(Module *m, int x, int y, int w, int h, int focused) {
  if (!terminal_live) {
    /* placeholder text before terminal starts */
    drw_setscheme(drw, scheme[0]);
    const char *msg = "profanity";
    int tw = drw_fontset_getwidth(drw, msg);
    int fh = drw->fonts->h;
    drw_text(drw, x + (w - tw) / 2, y + (h - fh) / 2, tw, fh, 0, msg, 0);
  }

  /* Create the container window only once.
   * No KeyPressMask – st gets keyboard directly. */
  if (prof_win == None && w > 0 && h > 0) {
    XSetWindowAttributes attrs;
    attrs.background_pixmap = None;
    attrs.event_mask = StructureNotifyMask | SubstructureNotifyMask |
                       ExposureMask | FocusChangeMask;

    prof_win = XCreateWindow(dpy, panel_win, x, y, w, h, 0, CopyFromParent,
                             InputOutput, CopyFromParent,
                             CWBackPixmap | CWEventMask, &attrs);
    profanity_win_export = prof_win;
    XMapWindow(dpy, prof_win);
    XFlush(dpy);

    struct timespec ts = {0, 100000000L}; /* 100 ms */
    nanosleep(&ts, NULL);

    char wid_str[32];
    snprintf(wid_str, sizeof(wid_str), "0x%lx", (unsigned long)prof_win);

    pid_t pid = fork();
    if (pid == 0) {
      XSetErrorHandler(dummy_handler);
      execlp("st", "st", "-w", wid_str, "-e", "profanity", NULL);
      _exit(1);
    } else if (pid > 0) {
      prof_pid = pid;
      terminal_live = 1;

      /* Wait for st's internal window to appear */
      int tries;
      for (tries = 0; tries < 50; tries++) {
        Window root_ret, parent_ret, *children;
        unsigned int nchildren;
        if (XQueryTree(dpy, prof_win, &root_ret, &parent_ret, &children,
                       &nchildren) &&
            nchildren > 0) {
          term_win = children[0];
          XFree(children);
          break;
        }
        if (children)
          XFree(children);
        struct timespec ts10 = {0, 10000000L}; /* 10 ms */
        nanosleep(&ts10, NULL);
      }

      if (term_win) {
        /* Map then focus – must be viewable first! */
        XMapWindow(dpy, term_win);
        XFlush(dpy);
        /* tiny delay to let the map take effect */
        struct timespec ts10 = {0, 20000000L}; /* 20 ms */
        nanosleep(&ts10, NULL);
        XSetInputFocus(dpy, term_win, RevertToPointerRoot, CurrentTime);
      }
      panel_redraw();
    }
  }

  /* Keep the container window positioned and sized. */
  if (prof_win) {
    profanity_x = panel_x + x;
    profanity_y = panel_y + y;
    profanity_w = w;
    profanity_h = h;

    XMoveResizeWindow(dpy, prof_win, x, y, w, h);

    if (term_win) {
      XMoveResizeWindow(dpy, term_win, 0, 0, w, h);
      XEvent cn;
      memset(&cn, 0, sizeof(cn));
      cn.type = ConfigureNotify;
      cn.xconfigure.window = term_win;
      cn.xconfigure.x = 0;
      cn.xconfigure.y = 0;
      cn.xconfigure.width = w;
      cn.xconfigure.height = h;
      cn.xconfigure.border_width = 0;
      cn.xconfigure.above = None;
      cn.xconfigure.override_redirect = False;
      XSendEvent(dpy, term_win, False, StructureNotifyMask, &cn);
    }
    XRaiseWindow(dpy, prof_win);
  }
}

void profanity_show(void) {
  if (term_win) {
    XMapWindow(dpy, prof_win);
    XMapWindow(dpy, term_win);
    XRaiseWindow(dpy, prof_win);
    XFlush(dpy);
    XSetInputFocus(dpy, term_win, RevertToPointerRoot, CurrentTime);
  }
}

void profanity_hide(void) {
  if (prof_win)
    XUnmapWindow(dpy, prof_win);
}

static void profanity_input(Module *m, const InputEvent *ev) {
  if (ev->type == EV_PRESS && term_win) {
    XSetInputFocus(dpy, term_win, RevertToPointerRoot, CurrentTime);
  }
}

static void profanity_destroy(Module *m) { kill_profanity(); }

static void profanity_timer(Module *m) {
  if (prof_pid > 0) {
    int status;
    if (waitpid(prof_pid, &status, WNOHANG) == prof_pid) {
      kill_profanity();
      panel_redraw();
    }
  }
}

static LayoutHints *profanity_hints(Module *m) {
  static LayoutHints hints = {.min_h = 80, .expand_x = 1, .expand_y = 1};
  return &hints;
}

Module profanity_module = {
    .name = "profanity",
    .init = profanity_init,
    .draw = profanity_draw,
    .input = profanity_input,
    .timer = profanity_timer,
    .destroy = profanity_destroy,
    .get_hints = profanity_hints,
    .theme = NULL,
    .margin_top = 0,
    .margin_bottom = 4, /* Help avoid last line clipping */
    .margin_left = 0,
    .margin_right = 0,
    .has_window = 0,
};

void __attribute__((constructor)) profanity_register(void) {
  register_module(&profanity_module);
}
