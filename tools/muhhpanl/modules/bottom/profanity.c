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
static Window prof_win = None;
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
  /* do NOT set has_window = 1; we want the container to call draw() every time
   */
}

static void profanity_draw(Module *m, int x, int y, int w, int h, int focused) {
  if (!terminal_live) {
    /* draw placeholder text until the terminal starts */
    drw_setscheme(drw, scheme[0]);
    const char *msg = "profanity";
    int tw = drw_fontset_getwidth(drw, msg);
    int fh = drw->fonts->h;
    drw_text(drw, x + (w - tw) / 2, y + (h - fh) / 2, tw, fh, 0, msg, 0);
  }

  /* create the child window exactly once, after we have a valid size */
  if (prof_win == None && w > 0 && h > 0) {
    XSetWindowAttributes attrs;
    attrs.background_pixmap = None; /* transparent */
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       KeyPressMask | PointerMotionMask | FocusChangeMask;

    /* Use CopyFromParent so depth/visual match the panel automatically */
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
      /* the terminal is now running – future draw calls will move/resize it */
      panel_redraw();
    }
  }

  /* every frame, keep the child window positioned and sized correctly */
  if (prof_win) {
    profanity_x = panel_x + x;
    profanity_y = panel_y + y;
    profanity_w = w;
    profanity_h = h;

    XMoveResizeWindow(dpy, prof_win, x, y, w, h);

    /* synthetic ConfigureNotify so the embedded terminal knows its real size */
    XEvent cn;
    memset(&cn, 0, sizeof(cn));
    cn.type = ConfigureNotify;
    cn.xconfigure.window = prof_win;
    cn.xconfigure.x = x;
    cn.xconfigure.y = y;
    cn.xconfigure.width = w;
    cn.xconfigure.height = h;
    cn.xconfigure.border_width = 0;
    cn.xconfigure.above = None;
    cn.xconfigure.override_redirect = False;
    XSendEvent(dpy, prof_win, False, StructureNotifyMask, &cn);

    XRaiseWindow(dpy, prof_win);
  }
}

void profanity_show(void) {
  if (prof_win) {
    XMapWindow(dpy, prof_win);
    XRaiseWindow(dpy, prof_win);
  }
}

void profanity_hide(void) {
  if (prof_win)
    XUnmapWindow(dpy, prof_win);
}

static void profanity_input(Module *m, const InputEvent *ev) {
  if (ev->type == EV_PRESS && prof_win && terminal_live) {
    XSetInputFocus(dpy, prof_win, RevertToPointerRoot, CurrentTime);
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
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
    .has_window =
        0, /* we manage the child window ourselves, but draw every frame */
};

void __attribute__((constructor)) profanity_register(void) {
  register_module(&profanity_module);
}
