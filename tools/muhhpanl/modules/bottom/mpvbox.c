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

int mpvbox_x, mpvbox_y, mpvbox_w, mpvbox_h;
Window mpv_win_export = None;

static pid_t mpv_pid = 0;
static Window mpv_win = None; /* child of panel_win */
static int is_playing = 0;
static char last_path[512] = "";

static void kill_mpv(void) {
  if (mpv_pid > 0) {
    kill(mpv_pid, SIGTERM);
    waitpid(mpv_pid, NULL, 0);
    mpv_pid = 0;
  }
  if (mpv_win) {
    XDestroyWindow(dpy, mpv_win);
    mpv_win = None;
    mpv_win_export = None;
  }
  is_playing = 0;
}

void mpvbox_play(const char *path) {
  if (!path || !path[0])
    return;
  if (is_playing)
    kill_mpv();
  strncpy(last_path, path, sizeof(last_path) - 1);

  /* create a child window inside the panel – safe visual/depth */
  XSetWindowAttributes attrs;
  attrs.background_pixmap = None; /* transparent, avoids BadPixmap */
  attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     KeyPressMask | PointerMotionMask | FocusChangeMask;

  mpv_win = XCreateWindow(dpy, panel_win, 0, 0, 10, 10, 0, CopyFromParent,
                          InputOutput, CopyFromParent,
                          CWBackPixmap | CWEventMask, &attrs);
  mpv_win_export = mpv_win;
  XMapWindow(dpy, mpv_win);
  XSetInputFocus(dpy, mpv_win, RevertToPointerRoot,
                 CurrentTime); /* give keyboard to mpv */

  char wid_arg[64];
  snprintf(wid_arg, sizeof(wid_arg), "--wid=%lu", (unsigned long)mpv_win);

  int is_youtube = (strstr(path, "youtube.com") || strstr(path, "youtu.be"));
  char ytdl_fmt[128] = "";
  if (is_youtube) {
    int height = 0;
    sscanf(MPV_YOUTUBE_QUALITY, "%dp", &height);
    if (height > 0)
      snprintf(ytdl_fmt, sizeof(ytdl_fmt),
               "--ytdl-format=bestvideo[height<=?%d]+bestaudio/best", height);
  }

  pid_t pid = fork();
  if (pid == 0) {
    int logfd = open("/tmp/mpvbox.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (logfd >= 0) {
      dup2(logfd, 2);
      close(logfd);
    }
    struct timespec ts = {0, 100000000L};
    nanosleep(&ts, NULL);
    if (is_youtube && ytdl_fmt[0]) {
      execlp("mpv", "mpv", wid_arg, "--osc=yes", "--ontop", "--volume=100",
             "--no-terminal", ytdl_fmt, path, NULL);
    } else {
      execlp("mpv", "mpv", wid_arg, "--osc=yes", "--ontop", "--volume=100",
             "--no-terminal", path, NULL);
    }
    _exit(1);
  } else if (pid > 0) {
    mpv_pid = pid;
    is_playing = 1;
  }
}

void mpvbox_stop(void) {
  kill_mpv();
  panel_redraw();
}
int mpvbox_is_playing(void) { return is_playing; }

void mpvbox_show(void) {
  if (mpv_win) {
    XMapWindow(dpy, mpv_win);
    XRaiseWindow(dpy, mpv_win);
  }
}
void mpvbox_hide(void) {
  if (mpv_win)
    XUnmapWindow(dpy, mpv_win);
}

static void mpvbox_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
}

static void mpvbox_draw(Module *m, int x, int y, int w, int h, int focused) {
  int pad = MODULE_PADDING;
  int inner_x = x + pad;
  int inner_y = y + pad;
  int inner_w = w - 2 * pad;
  int inner_h = h - 2 * pad;

  if (!is_playing) {
    XSetForeground(dpy, drw->gc, 0x3B4252);
    XFillRectangle(dpy, drw->drawable, drw->gc, inner_x, inner_y, inner_w,
                   inner_h);
    drw_setscheme(drw, scheme[0]);
    const char *msg = "No video :/";
    int fh = drw->fonts->h;
    int tw = drw_fontset_getwidth(drw, msg);
    drw_text(drw, inner_x + (inner_w - tw) / 2, inner_y + (inner_h - fh) / 2,
             tw, fh, 0, msg, 0);
    return;
  }

  if (mpv_win) {
    /* fill the entire module area, minus 4px safety at the bottom */
    int safety = 4;
    int vw = inner_w;
    int vh = inner_h - safety;
    if (vh < 1)
      vh = 1;

    /* child window coordinates are relative to the panel */
    XMoveResizeWindow(dpy, mpv_win, inner_x, inner_y, vw, vh);
    XRaiseWindow(dpy, mpv_win);

    /* store screen‑absolute coordinates for event replay */
    mpvbox_x = panel_x + inner_x;
    mpvbox_y = panel_y + inner_y;
    mpvbox_w = vw;
    mpvbox_h = vh;
  }
}

static void mpvbox_timer(Module *m) {
  if (mpv_pid > 0) {
    int status;
    if (waitpid(mpv_pid, &status, WNOHANG) == mpv_pid) {
      XDestroyWindow(dpy, mpv_win);
      mpv_win = None;
      mpv_win_export = None;
      mpv_pid = 0;
      is_playing = 0;
      panel_redraw();
    }
  }
}

static void mpvbox_destroy(Module *m) { kill_mpv(); }

static LayoutHints *mpvbox_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 240, .pref_h = 360, .expand_x = 1, .expand_y = 1};
  return &hints;
}

Module mpvbox_module = {
    .name = M_MPVBOX,
    .init = mpvbox_init,
    .draw = mpvbox_draw,
    .timer = mpvbox_timer,
    .destroy = mpvbox_destroy,
    .get_hints = mpvbox_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 4,
    .margin_right = 4,
};

void __attribute__((constructor)) mpvbox_register(void) {
  register_module(&mpvbox_module);
}
