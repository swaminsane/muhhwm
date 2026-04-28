#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
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
static Window mpv_win = None;
static int is_playing = 0;
static char last_path[512] = "";
static time_t play_start_time = 0;
static char current_title[256] = "";

/* ── now‑playing atom ─────────────────────────── */
static void set_now_playing(const char *title) {
  Atom a = XInternAtom(dpy, "_MUHH_MPV_TITLE", False);
  if (!title || !*title) {
    XDeleteProperty(dpy, root, a);
  } else {
    XChangeProperty(dpy, root, a, XA_STRING, 8, PropModeReplace,
                    (unsigned char *)title, (int)strlen(title));
  }
  XFlush(dpy);
}

/* ── watch logging ────────────────────────────── */
static void log_watch(const char *path, const char *title,
                      long long duration_sec) {
  const char *home = getenv("HOME");
  if (!home)
    return;
  char dir[512], logpath[1024];
  snprintf(dir, sizeof(dir), "%s/.cache/muhhwm", home);
  mkdir(dir, 0755);
  snprintf(logpath, sizeof(logpath), "%s/watchlog", dir);

  FILE *f = fopen(logpath, "a");
  if (!f)
    return;

  time_t now = time(NULL);
  char timestr[32];
  strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

  fprintf(f, "[%s] %s | %s | watched %lld sec\n", timestr, path,
          title ? title : "unknown", duration_sec);
  fclose(f);
}

/* ── kill mpv (updated) ───────────────────────── */
static void kill_mpv(void) {
  if (mpv_pid > 0) {
    kill(mpv_pid, SIGTERM);
    waitpid(mpv_pid, NULL, 0);
    mpv_pid = 0;

    /* log */
    long long duration = (long long)(time(NULL) - play_start_time);
    log_watch(last_path, current_title, duration);

    /* clear now‑playing */
    set_now_playing(NULL);
  }
  if (mpv_win) {
    XDestroyWindow(dpy, mpv_win);
    mpv_win = None;
    mpv_win_export = None;
  }
  is_playing = 0;
  current_title[0] = '\0';
}

/* ── titled play (new) ────────────────────────── */
void mpvbox_play_mode(const char *path, const char *mode, const char *title) {
  if (!path || !path[0])
    return;
  if (is_playing)
    kill_mpv();
  strncpy(last_path, path, sizeof(last_path) - 1);
  strncpy(current_title, title ? title : path, sizeof(current_title) - 1);
  play_start_time = time(NULL);

  /* set now‑playing atom */
  set_now_playing(current_title);

  XSetWindowAttributes attrs;
  attrs.background_pixmap = None;
  attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     KeyPressMask | PointerMotionMask | FocusChangeMask;

  mpv_win = XCreateWindow(dpy, panel_win, 0, 0, 10, 10, 0, CopyFromParent,
                          InputOutput, CopyFromParent,
                          CWBackPixmap | CWEventMask, &attrs);
  mpv_win_export = mpv_win;
  XMapWindow(dpy, mpv_win);
  {
    XFlush(dpy);
    struct timespec ts20 = {0, 20000000L}; /* 20 ms */
    nanosleep(&ts20, NULL);
  }
  XSetInputFocus(dpy, mpv_win, RevertToPointerRoot, CurrentTime);

  char wid_arg[64];
  snprintf(wid_arg, sizeof(wid_arg), "--wid=%lu", (unsigned long)mpv_win);

  int is_youtube = (strstr(path, "youtube.com") || strstr(path, "youtu.be"));
  char ytdl_fmt[128] = "";

  if (is_youtube) {
    if (mode && strcmp(mode, "audio") == 0) {
      snprintf(ytdl_fmt, sizeof(ytdl_fmt), "--ytdl-format=bestaudio/best");
    } else {
      int height = 0;
      sscanf(MPV_YOUTUBE_QUALITY, "%dp", &height);
      if (height > 0)
        snprintf(ytdl_fmt, sizeof(ytdl_fmt),
                 "--ytdl-format=bestvideo[height<=?%d]+bestaudio/best", height);
      else
        snprintf(ytdl_fmt, sizeof(ytdl_fmt),
                 "--ytdl-format=bestvideo+bestaudio/best");
    }
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
      if (mode && strcmp(mode, "audio") == 0) {
        execlp("mpv", "mpv", wid_arg, "--osc=yes", "--force-window=yes",
               "--ontop", "--volume=100", "--no-terminal", "--vid=no", ytdl_fmt,
               path, NULL);
      } else {
        execlp("mpv", "mpv", wid_arg, "--osc=yes", "--force-window=yes",
               "--ontop", "--volume=100", "--no-terminal", ytdl_fmt, path,
               NULL);
      }
    } else {
      execlp("mpv", "mpv", wid_arg, "--osc=yes", "--force-window=yes",
             "--ontop", "--volume=100", "--no-terminal", path, NULL);
    }
    _exit(1);
  } else if (pid > 0) {
    mpv_pid = pid;
    is_playing = 1;
  }
}

/* ── default play (no title) ──────────────────── */
void mpvbox_play(const char *path) { mpvbox_play_mode(path, "video", NULL); }

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

/* ── standard module boilerplate ──────────────── */
static void mpvbox_init(Module *m, int x, int y, int w, int h) {
  m->w = w;
  m->h = h;
}

static void mpvbox_draw(Module *m, int x, int y, int w, int h, int focused) {
  if (chill_mode) {
    /* override with full main area */
    x = 0;  /* relative to panel_win */
    y = 20; /* below topstrip */
    w = panel_w;
    h = panel_h - 20 - 13; /* topstrip (20) + timeline (13) */
  }

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
    int safety = 4;
    int vw = inner_w;
    int vh = inner_h - safety;
    if (vh < 1)
      vh = 1;

    XMoveResizeWindow(dpy, mpv_win, inner_x, inner_y, vw, vh);
    XRaiseWindow(dpy, mpv_win);

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
      /* same logging as kill_mpv */
      long long duration = (long long)(time(NULL) - play_start_time);
      log_watch(last_path, current_title, duration);
      set_now_playing(NULL);

      XDestroyWindow(dpy, mpv_win);
      mpv_win = None;
      mpv_win_export = None;
      mpv_pid = 0;
      is_playing = 0;
      current_title[0] = '\0';
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
