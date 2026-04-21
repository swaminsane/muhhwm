/* muhhbar.c - muhhwm statusbar, standalone X window */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"
#include "modules.h"
#include "muhhbar.h"
#include "util.h"

static const char *fonts[] = {
    "DejaVu Sans Mono:size=10",
};
static const char *colors[][2] = {
    [SchNorm] = {"#dcd7ba", "#1f1f28"},   [SchAccent] = {"#1f1f28", "#7e9cd8"},
    [SchBlock] = {"#1f1f28", "#727169"},  [SchWarn] = {"#c0a36e", "#1f1f28"},
    [SchCrit] = {"#c34043", "#1f1f28"},   [SchGreen] = {"#76946a", "#1f1f28"},
    [SchGrey] = {"#727169", "#1f1f28"},   [SchNsStudy] = {"#7e9cd8", "#1f1f28"},
    [SchNsCode] = {"#76946a", "#1f1f28"}, [SchNsFree] = {"#957fb8", "#1f1f28"},
};

/* ── shared globals ──────────────────────────────────────────────────── */
Display *dpy;
Window root;
int screen;
Drw *drw;
Clr **scheme;
int lrpad;
int barh;
int barw;
int detail_view = 0;
time_t detail_time = 0;

/* ── private ─────────────────────────────────────────────────────────── */
static Window barwin;
static int sw, sh;
static int curmode = MODE_SYSTEM;
static int running = 1;

/* ── spawn ───────────────────────────────────────────────────────────── */
void spawn(const char **cmd) {
  pid_t pid = fork();
  if (pid < 0)
    return;
  if (pid == 0) {
    setsid();
    execvp(cmd[0], (char *const *)cmd);
    _exit(1);
  }
}

static void sigchld(int s) {
  (void)s;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

/* ── draw ────────────────────────────────────────────────────────────── */
static void draw(void) {
  int i;

  /* check detail view timeout */
  if (detail_view && time(NULL) - detail_time >= DETAIL_TIMEOUT)
    detail_view = 0;

  drw_setscheme(drw, scheme[SchNorm]);
  drw_rect(drw, 0, 0, (unsigned int)barw, (unsigned int)barh, 1, 1);

  /* detail view: activity takes full bar, no mode blocks */
  if (detail_view) {
    focusmods[MOD_ACTIVITY].draw(0);
    drw_map(drw, barwin, 0, 0, (unsigned int)barw, (unsigned int)barh);
    return;
  }

  /* normal mode drawing */
  if (curmode == MODE_SYSTEM) {
    int x = 0;
    for (i = 0; i < NSYSMODS; i++) {
      sysmods[i].x = x;
      x = sysmods[i].draw(x);
    }
  } else if (curmode == MODE_FOCUS) {
    int x = 0;
    for (i = 0; i < NFOCUSMODS; i++) {
      focusmods[i].x = x;
      x = focusmods[i].draw(x);
    }
  } else if (curmode == MODE_MEDIA) {
    int x = 0;
    for (i = 0; i < NMEDIAMODS; i++) {
      mediamods[i].x = x;
      x = mediamods[i].draw(x);
    }
  }

  /* mode blocks */
  int bx = barw - BPAD - (NMODES * (BSQW + BSQGAP) - BSQGAP);
  int by = (barh - BSQH) / 2;
  for (i = 0; i < NMODES; i++) {
    drw_setscheme(drw, scheme[i == curmode ? SchAccent : SchBlock]);
    drw_rect(drw, bx + i * (BSQW + BSQGAP), by, BSQW, BSQH, 1, 1);
  }

  drw_map(drw, barwin, 0, 0, (unsigned int)barw, (unsigned int)barh);
}

/* ── input ───────────────────────────────────────────────────────────── */
static void handle_button(XButtonEvent *ev) {
  int ex = ev->x;
  int button = (int)ev->button;
  int i;

  /* detail view: any click collapses it */
  if (detail_view) {
    detail_view = 0;
    draw();
    return;
  }

  /* mode blocks */
  int bx = barw - BPAD - (NMODES * (BSQW + BSQGAP) - BSQGAP);
  for (i = 0; i < NMODES; i++) {
    int bs = bx + i * (BSQW + BSQGAP);
    if (ex >= bs && ex < bs + BSQW) {
      curmode = i;
      int new_w = 0, j;
      if (curmode == MODE_SYSTEM)
        for (j = 0; j < NSYSMODS; j++)
          new_w += sysmods[j].width;
      else if (curmode == MODE_FOCUS)
        for (j = 0; j < NFOCUSMODS; j++)
          new_w += focusmods[j].width;
      new_w += BLOCK_TOTAL;
      barw = new_w;
      drw_resize(drw, (unsigned int)barw, (unsigned int)barh);
      XMoveResizeWindow(dpy, barwin, sw - barw, sh - barh, (unsigned int)barw,
                        (unsigned int)barh);
      XClearWindow(dpy, barwin);
      XFlush(dpy);
      draw();
      return;
    }
  }

  if (curmode == MODE_SYSTEM) {
    for (i = 0; i < NSYSMODS; i++) {
      if (ex >= sysmods[i].x && ex < sysmods[i].x + sysmods[i].width) {
        if (sysmods[i].click)
          sysmods[i].click(button);
        draw();
        return;
      }
    }
  } else if (curmode == MODE_FOCUS) {
    for (i = 0; i < NFOCUSMODS; i++) {
      if (ex >= focusmods[i].x && ex < focusmods[i].x + focusmods[i].width) {
        if (focusmods[i].click)
          focusmods[i].click(button);
        draw();
        return;
      }
    }
  } else if (curmode == MODE_MEDIA) {
    for (i = 0; i < NMEDIAMODS; i++) {
      if (ex >= mediamods[i].x && ex < mediamods[i].x + mediamods[i].width) {
        if (mediamods[i].click)
          mediamods[i].click(button);
        draw();
        return;
      }
    }
  }
}

static void handle_scroll(XButtonEvent *ev) {
  int ex = ev->x;
  int dir = (ev->button == Button4) ? +1 : -1;
  int i;

  if (detail_view)
    return;

  if (curmode == MODE_SYSTEM) {
    for (i = 0; i < NSYSMODS; i++) {
      if (ex >= sysmods[i].x && ex < sysmods[i].x + sysmods[i].width) {
        if (sysmods[i].scroll)
          sysmods[i].scroll(dir);
        draw();
        return;
      }
    }
  } else if (curmode == MODE_FOCUS) {
    for (i = 0; i < NFOCUSMODS; i++) {
      if (ex >= focusmods[i].x && ex < focusmods[i].x + focusmods[i].width) {
        if (focusmods[i].scroll)
          focusmods[i].scroll(dir);
        draw();
        return;
      }
    }
  } else if (curmode == MODE_MEDIA) {
    for (i = 0; i < NMEDIAMODS; i++) {
      if (ex >= mediamods[i].x && ex < mediamods[i].x + mediamods[i].width) {
        if (mediamods[i].scroll)
          mediamods[i].scroll(dir);
        draw();
        return;
      }
    }
  }
}

/* ── main ────────────────────────────────────────────────────────────── */
int main(void) {
  int i;

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sa.sa_handler = sigchld;
  sigaction(SIGCHLD, &sa, NULL);

  dpy = XOpenDisplay(NULL);
  if (!dpy)
    die("muhhbar: cannot open display");

  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);

  drw = drw_create(dpy, screen, root, (unsigned int)sw, 1);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("muhhbar: no fonts loaded");

  lrpad = (int)drw->fonts->h;
  barh = (int)drw->fonts->h + 2;

  scheme = ecalloc(SchLast, sizeof(Clr *));
  for (i = 0; i < SchLast; i++)
    scheme[i] = drw_scm_create(drw, colors[i], 2);

  modules_init();
  focus_modules_init();
  media_modules_init();

  barw = 0;
  for (i = 0; i < NSYSMODS; i++)
    barw += sysmods[i].width;
  barw += BLOCK_TOTAL;

  int barx = sw - barw;
  int bary = sh - barh;

  drw_resize(drw, (unsigned int)barw, (unsigned int)barh);

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = scheme[SchNorm][ColBg].pixel;
  wa.event_mask = ButtonPressMask | ExposureMask;

  barwin = XCreateWindow(dpy, root, barx, bary, (unsigned int)barw,
                         (unsigned int)barh, 0, DefaultDepth(dpy, screen),
                         CopyFromParent, DefaultVisual(dpy, screen),
                         CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);

  XClassHint ch = {"muhhbar", "muhhbar"};
  XSetClassHint(dpy, barwin, &ch);
  XMapRaised(dpy, barwin);
  XFlush(dpy);

  int xfd = ConnectionNumber(dpy);
  draw();

  while (running) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(xfd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    select(xfd + 1, &fds, NULL, NULL, &tv);

    time_t now = time(NULL);
    int redraw = 0;

    if (detail_view) {
      /* in detail view just redraw on tick to update timeout */
      redraw = 1;
    } else if (curmode == MODE_SYSTEM) {
      for (i = 0; i < NSYSMODS; i++) {
        if (now - sysmods[i].updated >= (time_t)sysmods[i].interval) {
          sysmods[i].update();
          sysmods[i].updated = now;
          redraw = 1;
        }
      }
    } else if (curmode == MODE_FOCUS) {
      for (i = 0; i < NFOCUSMODS; i++) {
        if (now - focusmods[i].updated >= (time_t)focusmods[i].interval) {
          focusmods[i].update();
          focusmods[i].updated = now;
          redraw = 1;
        }
      }
    } else if (curmode == MODE_MEDIA) {
      for (i = 0; i < NMEDIAMODS; i++) {
        if (now - mediamods[i].updated >= (time_t)mediamods[i].interval) {
          mediamods[i].update();
          mediamods[i].updated = now;
          redraw = 1;
        }
      }
    }

    while (XPending(dpy)) {
      XEvent ev;
      XNextEvent(dpy, &ev);
      switch (ev.type) {
      case Expose:
        redraw = 1;
        break;
      case ButtonPress:
        if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5)
          handle_scroll(&ev.xbutton);
        else
          handle_button(&ev.xbutton);
        break;
      }
    }

    if (redraw)
      draw();
  }

  for (i = 0; i < SchLast; i++)
    free(scheme[i]);
  free(scheme);
  drw_free(drw);
  XDestroyWindow(dpy, barwin);
  XCloseDisplay(dpy);
  return 0;
}
