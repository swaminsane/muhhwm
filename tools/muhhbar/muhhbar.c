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

/* muhhbar.h brings in drw.h, colors.h, scheme enum, FONT_MAIN */
#include "config.h"
#include "modules.h"
#include "muhhbar.h"
#include "util.h"

/* fonts and colors — must come after muhhbar.h (scheme enum + COL_ macros) */
static const char *fonts[] = {
    FONT_MAIN, "Noto Color Emoji:pixelsize=11:antialias=true:autohint=true"};
static const char *colors[][2] = {
    [SchNorm] = {COL_FG, COL_BG},
    [SchAccent] = {COL_BG, COL_ACCENT},
    [SchBlock] = {COL_BG, COL_BRIGHT_BLACK},
    [SchWarn] = {COL_YELLOW, COL_BG},
    [SchCrit] = {COL_RED, COL_BG},
    [SchGreen] = {COL_GREEN, COL_BG},
    [SchGrey] = {COL_BRIGHT_BLACK, COL_BG},
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

  drw_setscheme(drw, scheme[SchNorm]);
  drw_rect(drw, 0, 0, (unsigned int)barw, (unsigned int)barh, 1, 1);

  if (curmode == MODE_SYSTEM) {
    int x = 0;
    for (i = 0; i < NSYSMODS; i++) {
      sysmods[i].x = x;
      x = sysmods[i].draw(x);
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

  /* mode blocks */
  int bx = barw - BPAD - (NMODES * (BSQW + BSQGAP) - BSQGAP);
  for (i = 0; i < NMODES; i++) {
    int bs = bx + i * (BSQW + BSQGAP);
    if (ex >= bs && ex < bs + BSQW) {
      curmode = i;
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
  }
}

static void handle_scroll(XButtonEvent *ev) {
  int ex = ev->x;
  int dir = (ev->button == Button4) ? +1 : -1;
  int i;

  if (curmode == MODE_SYSTEM) {
    for (i = 0; i < NSYSMODS; i++) {
      if (ex >= sysmods[i].x && ex < sysmods[i].x + sysmods[i].width) {
        if (sysmods[i].scroll)
          sysmods[i].scroll(dir);
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

  barw = BLOCK_TOTAL;
  for (i = 0; i < NSYSMODS; i++)
    barw += sysmods[i].width;

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

    if (curmode == MODE_SYSTEM) {
      for (i = 0; i < NSYSMODS; i++) {
        if (now - sysmods[i].updated >= (time_t)sysmods[i].interval) {
          sysmods[i].update();
          sysmods[i].updated = now;
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
