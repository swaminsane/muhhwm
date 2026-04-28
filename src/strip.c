
/* strip.c - muhhwm day strip
 *
 * A 3px wide vertical strip on the left edge of the screen showing
 * namespace activity per minute throughout the day.
 *
 * Data file: ~/.cache/muhhwm/daydata
 * Format:    YYYY-MM-DD MINUTE NS
 *   MINUTE = 0-1439 (minute of day)
 *   NS     = 0 (no data/grey), 1 (study), 2 (code), 3 (free)
 *
 * One line appended per minute. Never rewritten. fsync after every write.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"
#include "muhh.h"
#include "strip.h"

/* ── constants ───────────────────────────────────────────────────────────── */

#define STRIP_W 4          /* normal width px                           */
#define STRIP_W_HOVER 12   /* hover width px                            */
#define STRIP_MINUTES 1440 /* minutes in a day                          */
#define HOVER_DIST 10      /* px from left edge to trigger hover        */
#define SYNC_FLASH_MS 200  /* ms the sync indicator stays lit           */

/* namespace colors — not from colors.h, fixed aesthetic choices */
#define COL_NS_NONE 0x3D3D3D    /* grey — no data                           */
#define COL_NS_STUDY 0xC0392B   /* deep red                                 */
#define COL_NS_CODE 0x2980B9    /* medium blue                              */
#define COL_NS_FREE 0x27AE60    /* rich green                               */
#define COL_NS_STUDY_B 0xE74C3C /* bright red — current point               */
#define COL_NS_CODE_B 0x3498DB  /* bright blue — current point              */
#define COL_NS_FREE_B 0x2ECC71  /* bright green — current point             */

/* ── state ───────────────────────────────────────────────────────────────── */

static Window strip_xwin = None;
static int strip_cur_ns = 0; /* current namespace 0/1/2        */
static unsigned char
    strip_data[STRIP_MINUTES];           /* 0=grey,1=study,2=code,3=free */
static char strip_date[11];              /* "2026-04-23"                   */
static int strip_hover = 0;              /* 1 if mouse near left edge      */
static struct timespec strip_last_write; /* for sync flash                 */
static int strip_h = 0;                  /* height of strip window         */
static int strip_y = 0;                  /* y position of strip window     */
static int strip_visible = 1;            /* initially shown */
static GC strip_gc = None;

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void get_today(char *out) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(out, 11, "%Y-%m-%d", tm);
}

static int minute_of_day(void) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  return tm->tm_hour * 60 + tm->tm_min;
}

static void data_path(char *out, int outlen) {
  const char *home = getenv("HOME");
  if (!home)
    home = "/tmp";
  snprintf(out, (size_t)outlen, "%s/.cache/muhhwm/daydata", home);
}

static void ensure_dir(void) {
  const char *home = getenv("HOME");
  if (!home)
    return;
  char dir[512];
  snprintf(dir, sizeof(dir), "%s/.cache/muhhwm", home);
  /* mkdir -p equivalent — ignore EEXIST */
  mkdir(dir, 0755);
}

static unsigned long ns_color(int ns, int bright) {
  switch (ns) {
  case 1:
    return bright ? COL_NS_STUDY_B : COL_NS_STUDY;
  case 2:
    return bright ? COL_NS_CODE_B : COL_NS_CODE;
  case 3:
    return bright ? COL_NS_FREE_B : COL_NS_FREE;
  default:
    return COL_NS_NONE;
  }
}

/* blend two 0xRRGGBB colors by factor 0-256 (0=all a, 256=all b) */
static unsigned long blend_color(unsigned long a, unsigned long b, int factor) {
  int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
  int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
  int r = ar + (br - ar) * factor / 256;
  int g = ag + (bg - ag) * factor / 256;
  int bv = ab + (bb - ab) * factor / 256;
  return ((unsigned long)r << 16) | ((unsigned long)g << 8) | (unsigned long)bv;
}

static unsigned long xcolor(unsigned long rgb) {
  XColor c;
  c.red = ((rgb >> 16) & 0xFF) * 257;
  c.green = ((rgb >> 8) & 0xFF) * 257;
  c.blue = (rgb & 0xFF) * 257;
  c.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(wm.dpy, DefaultColormap(wm.dpy, wm.screen), &c);
  return c.pixel;
}

/* ── data I/O ────────────────────────────────────────────────────────────── */

static void data_load_today(void) {
  char path[512];
  data_path(path, sizeof(path));

  char today[11];
  get_today(today);
  strncpy(strip_date, today, sizeof(strip_date));

  memset(strip_data, 0, sizeof(strip_data));

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[64];
  while (fgets(line, sizeof(line), f)) {
    char date[11];
    int minute, ns;
    if (sscanf(line, "%10s %d %d", date, &minute, &ns) != 3)
      continue;
    if (strcmp(date, today) != 0)
      continue;
    if (minute < 0 || minute >= STRIP_MINUTES)
      continue;
    if (ns < 0 || ns > 3)
      continue;
    strip_data[minute] = (unsigned char)ns;
  }
  fclose(f);

  /* fill any minute from midnight to now that has no data with 0 (grey)
   * — this handles gaps from suspend, crash, etc. They're already 0
   * from memset so nothing extra needed. */
}

static void data_write_minute(int minute, int ns) {
  char path[512];
  data_path(path, sizeof(path));

  ensure_dir();

  FILE *f = fopen(path, "a");
  if (!f)
    return;

  fprintf(f, "%s %d %d\n", strip_date, minute, ns);
  fflush(f);
  fsync(fileno(f)); /* flush to disk — survives hard poweroff */
  fclose(f);

  clock_gettime(CLOCK_MONOTONIC, &strip_last_write);
}

/* ── strip window ────────────────────────────────────────────────────────── */

void strip_init(void) {
  ensure_dir();
  data_load_today();

  /* calculate geometry */
  int bh = wm.drw->fonts->h + 2;
  if (topbar) {
    strip_y = bh;
    strip_h = wm.sh - bh;
  } else {
    strip_y = 0;
    strip_h = wm.sh - bh;
  }

  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = xcolor(COL_NS_NONE);
  wa.event_mask =
      ButtonPressMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask;

  strip_xwin =
      XCreateWindow(wm.dpy, wm.root, 0, strip_y, STRIP_W, (unsigned int)strip_h,
                    0, DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
                    DefaultVisual(wm.dpy, wm.screen),
                    CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);

  strip_gc = XCreateGC(wm.dpy, strip_xwin, 0, NULL);

  XMapRaised(wm.dpy, strip_xwin);
  XFlush(wm.dpy);

  strip_set_ns(wm.selmon->ans);
  strip_draw();
}

Window strip_win(void) { return strip_xwin; }

int strip_window(void) { return strip_xwin != None; }

void strip_set_ns(int ns) {
  /* ns is 0-based namespace index from muhhwm, map to 1/2/3 */
  strip_cur_ns = ns + 1;
}

/* ── drawing ─────────────────────────────────────────────────────────────── */

void strip_draw(void) {
  if (strip_xwin == None || strip_h <= 0)
    return;

  int w = strip_hover ? STRIP_W_HOVER : STRIP_W;
  int cur_min = minute_of_day();

  /* resize window if hover state changed */
  XResizeWindow(wm.dpy, strip_xwin, (unsigned int)w, (unsigned int)strip_h);

  /* background color from COL_BG — use xcolor with a dark fallback */
  unsigned long bg = xcolor(COL_NS_NONE);

  /* each pixel row = strip_h / 1440 * minute — but 1440 > strip_h usually
   * so we map: y = minute * strip_h / 1440 */

  /* clear */
  XSetForeground(wm.dpy, strip_gc, bg);
  XFillRectangle(wm.dpy, strip_xwin, strip_gc, 0, 0, (unsigned int)w,
                 (unsigned int)strip_h);

  /* draw minute segments */
  for (int m = 0; m < STRIP_MINUTES; m++) {
    int y1 = m * strip_h / STRIP_MINUTES;
    int y2 = (m + 1) * strip_h / STRIP_MINUTES;
    if (y2 <= y1)
      y2 = y1 + 1;
    int seg_h = y2 - y1;

    unsigned long col;
    if (m > cur_min) {
      /* future — blend bg with grey at 30% */
      col = xcolor(blend_color(COL_NS_NONE, 0x1A1A1A, 77));
    } else {
      col = xcolor(ns_color(strip_data[m], 0));
    }

    XSetForeground(wm.dpy, strip_gc, col);
    XFillRectangle(wm.dpy, strip_xwin, strip_gc, 0, y1, (unsigned int)w,
                   (unsigned int)seg_h);
  }

  /* hour marks */
  for (int hour = 1; hour < 24; hour++) {
    int m = hour * 60;
    int y = m * strip_h / STRIP_MINUTES;
    int is_cur_hour = (cur_min / 60 == hour);
    unsigned long hcol = xcolor(is_cur_hour ? 0x888888 : 0x555555);
    int mark_h = is_cur_hour ? 2 : 1;
    XSetForeground(wm.dpy, strip_gc, hcol);
    XFillRectangle(wm.dpy, strip_xwin, strip_gc, 0, y, (unsigned int)w,
                   (unsigned int)mark_h);
  }

  /* current moment — 3×3 bright point */
  {
    int y = cur_min * strip_h / STRIP_MINUTES;
    int bright_col = xcolor(ns_color(strip_cur_ns, 1));
    XSetForeground(wm.dpy, strip_gc, bright_col);
    int py = y - 1;
    if (py < 0)
      py = 0;
    XFillRectangle(wm.dpy, strip_xwin, strip_gc, 0, py, (unsigned int)w, 3);
  }

  /* sync flash — top 1px bright for SYNC_FLASH_MS after last write */
  {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - strip_last_write.tv_sec) * 1000 +
                      (now.tv_nsec - strip_last_write.tv_nsec) / 1000000;
    if (elapsed_ms < SYNC_FLASH_MS) {
      XSetForeground(wm.dpy, strip_gc, xcolor(0xFFFFFF));
      XFillRectangle(wm.dpy, strip_xwin, strip_gc, 0, 0, (unsigned int)w, 1);
    }
  }

  XFlush(wm.dpy);
}

/* ── tick — called every minute ──────────────────────────────────────────── */

void strip_tick(void) {
  char today[11];
  get_today(today);

  /* date changed — new day */
  if (strcmp(today, strip_date) != 0) {
    strncpy(strip_date, today, sizeof(strip_date));
    memset(strip_data, 0, sizeof(strip_data));
  }

  int m = minute_of_day();
  strip_data[m] = (unsigned char)strip_cur_ns;
  data_write_minute(m, strip_cur_ns);
  strip_draw();
}

/* ── hover ───────────────────────────────────────────────────────────────── */

void strip_motion(int x, int y) {
  (void)y;
  int was_hover = strip_hover;
  strip_hover = (x <= HOVER_DIST);
  if (strip_hover != was_hover)
    strip_draw();
}

/* ── click ───────────────────────────────────────────────────────────────── */

void strip_click(int y, int button) {
  int minute = y * STRIP_MINUTES / strip_h;
  if (minute < 0)
    minute = 0;
  if (minute >= STRIP_MINUTES)
    minute = STRIP_MINUTES - 1;

  int cur_min = minute_of_day();

  if (button == 1) {
    /* left click — show info in title bar area via stext */
    if (minute >= cur_min - 2 && minute <= cur_min + 2) {
      /* clicked near current moment — show today summary */
      int counts[4] = {0, 0, 0, 0};
      for (int i = 0; i <= cur_min; i++)
        counts[strip_data[i]]++;
      snprintf(stext, sizeof(stext),
               "today: study %dh%02dm  code %dh%02dm  free %dh%02dm",
               counts[1] / 60, counts[1] % 60, counts[2] / 60, counts[2] % 60,
               counts[3] / 60, counts[3] % 60);
    } else {
      /* clicked past segment — show what was happening */
      int h = minute / 60;
      int mn = minute % 60;
      const char *nsname;
      switch (strip_data[minute]) {
      case 1:
        nsname = "study";
        break;
      case 2:
        nsname = "code";
        break;
      case 3:
        nsname = "free";
        break;
      default:
        nsname = "idle";
        break;
      }
      /* count streak */
      int streak = 0;
      int ns = strip_data[minute];
      for (int i = minute; i >= 0 && strip_data[i] == ns; i--)
        streak++;
      snprintf(stext, sizeof(stext), "%02d:%02d — %s — %dmin streak", h, mn,
               nsname, streak);
    }
    /* redraw bar to show stext */
    for (Monitor *m = wm.mons; m; m = m->next)
      bar_draw(m);

  } else if (button == 3) {
    /* right click — open khal day view beside strip */
    const char *cmd[] = {"/bin/sh", "-c", "$HOME/.local/bin/menu/khalmenu",
                         NULL};
    Arg a = {.v = cmd};
    spawn(&a);
  }
}

void strip_toggle_visibility(void) {
  if (strip_xwin == None)
    return;

  if (strip_visible) {
    XUnmapWindow(wm.dpy, strip_xwin);
    strip_visible = 0;
  } else {
    XMapRaised(wm.dpy, strip_xwin);
    strip_visible = 1;
    strip_draw();
  }
}
