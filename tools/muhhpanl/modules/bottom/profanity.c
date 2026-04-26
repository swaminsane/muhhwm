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

static pid_t prof_pid = 0;
static Window prof_win = None;
static int terminal_live =
    0; /* 1 = terminal is running, switch to window mode */
static char debug_msg[256] = "init...";

static void kill_profanity(void) {
  if (prof_pid > 0) {
    kill(prof_pid, SIGTERM);
    waitpid(prof_pid, NULL, 0);
    prof_pid = 0;
  }
  if (prof_win) {
    XDestroyWindow(dpy, prof_win);
    prof_win = None;
  }
  terminal_live = 0;
}

static void profanity_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
  m->has_window = 0; /* initially we want the container to draw us */
  /* we will set margins to 0 later when the terminal is ready */
  snprintf(debug_msg, sizeof(debug_msg), "init w=%d h=%d", w, h);
}

static void profanity_draw(Module *m, int x, int y, int w, int h, int focused) {
  if (!terminal_live) {
    /* Phase 1: show debug text with a card background */
    if (m->theme) {
      XSetForeground(dpy, drw->gc, m->theme->bg);
      XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
      if (m->theme->border_w > 0) {
        XSetForeground(dpy, drw->gc, m->theme->border);
        for (int i = 0; i < m->theme->border_w; i++)
          XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i,
                         w - 1 - 2 * i, h - 1 - 2 * i);
      }
    }
    drw_setscheme(drw, scheme[0]);
    int tw = drw_fontset_getwidth(drw, debug_msg);
    int fh = drw->fonts->h;
    drw_text(drw, x + 5, y + (h - fh) / 2, tw, fh, 0, debug_msg, 0);
  }
  /* when terminal_live == 1, the container skips drawing us (has_window==1),
     so we don't need to draw anything here. */

  /* create child window once */
  if (prof_win == None && w > 0 && h > 0) {
    XSetWindowAttributes attrs;
    attrs.background_pixmap = None;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       KeyPressMask | PointerMotionMask;

    prof_win = XCreateWindow(dpy, panel_win, x, y, w, h, 0, CopyFromParent,
                             InputOutput, CopyFromParent,
                             CWBackPixmap | CWEventMask, &attrs);
    if (prof_win) {
      XMapWindow(dpy, prof_win);
      snprintf(debug_msg, sizeof(debug_msg), "child created 0x%lx",
               (unsigned long)prof_win);
    } else {
      snprintf(debug_msg, sizeof(debug_msg), "XCreateWindow failed");
    }
  }

  if (prof_win) {
    XMoveResizeWindow(dpy, prof_win, x, y, w, h);
  }

  /* launch terminal only once */
  if (!terminal_live && prof_win && w > 0 && h > 0) {
    snprintf(debug_msg, sizeof(debug_msg), "launching st...");

    char wid_str[32];
    snprintf(wid_str, sizeof(wid_str), "0x%lx", (unsigned long)prof_win);

    pid_t pid = fork();
    if (pid == 0) {
      struct timespec ts = {0, 100000000L};
      nanosleep(&ts, NULL);
      execlp("st", "st", "-w", wid_str, "-e", "profanity", NULL);
      _exit(1);
    } else if (pid > 0) {
      prof_pid = pid;
      terminal_live = 1; /* switch to window mode */

      /* tell the container to stop drawing us and to give us the full area */
      m->has_window = 1;
      m->theme = NULL;
      m->margin_top = 0;
      m->margin_bottom = 0;
      m->margin_left = 0;
      m->margin_right = 0;

      panel_redraw();
    }
  }
}

static void profanity_destroy(Module *m) { kill_profanity(); }

static void profanity_timer(Module *m) {
  if (prof_pid > 0) {
    int status;
    if (waitpid(prof_pid, &status, WNOHANG) == prof_pid) {
      prof_pid = 0;
      terminal_live = 0;
      XDestroyWindow(dpy, prof_win);
      prof_win = None;
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
    .timer = profanity_timer,
    .destroy = profanity_destroy,
    .get_hints = profanity_hints,
    /* initially we use a theme and margins so you can see the debug text */
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
    .has_window = 0,
};

void __attribute__((constructor)) profanity_register(void) {
  register_module(&profanity_module);
}
