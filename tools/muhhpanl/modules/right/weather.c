#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <dirent.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"

typedef struct {
  char **files; /* array of file paths in the weather directory */
  int nfiles;   /* how many files found */
  int cur_file; /* index of the currently displayed file */

  char text[10][128]; /* up to 10 lines of weather text */
  int nlines;         /* actual number of lines (after skipping blanks) */
  time_t last_update; /* mtime of the current file */

  /* blink transition */
  int transitioning;           /* 1 = clearing screen between cities */
  struct timespec trans_start; /* when the blink began */
} WeatherState;

/* expand a path that starts with ~ */
static void expand_path(const char *in, char *out, size_t sz) {
  if (in[0] == '~') {
    const char *home = getenv("HOME");
    snprintf(out, sz, "%s%s", home, in + 1);
  } else {
    strncpy(out, in, sz - 1);
  }
}

/* scan the weather directory and collect every city file */
static void scan_files(WeatherState *s) {
  if (s->files) {
    for (int i = 0; i < s->nfiles; i++)
      free(s->files[i]);
    free(s->files);
    s->files = NULL;
  }
  s->nfiles = 0;

  char dir[512];
  expand_path(WEATHER_DIR, dir, sizeof(dir));

  DIR *d = opendir(dir);
  if (!d)
    return;

  struct dirent *ent;
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.')
      continue; /* skip . and .. */

    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);

    struct stat st;
    if (stat(full, &st) != 0 || !S_ISREG(st.st_mode))
      continue;

    s->files = realloc(s->files, (s->nfiles + 1) * sizeof(char *));
    s->files[s->nfiles] = strdup(full);
    s->nfiles++;
  }
  closedir(d);
}

/* load a specific file, skip empty lines */
static void load_file(WeatherState *s, const char *path) {
  s->nlines = 0;
  s->last_update = 0;

  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f) && s->nlines < 10) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0')
      continue; /* skip blank lines */
    strncpy(s->text[s->nlines], line, sizeof(s->text[0]) - 1);
    s->nlines++;
  }
  fclose(f);

  struct stat st;
  if (stat(path, &st) == 0) {
    s->last_update = st.st_mtime;
  }
}

/* ── blink helpers ────────────────────────────────── */
static void start_blink(WeatherState *s) {
  s->transitioning = 1;
  clock_gettime(CLOCK_MONOTONIC, &s->trans_start);
}

static void finish_blink(WeatherState *s) {
  s->transitioning = 0;

  if (s->nfiles == 0)
    return;

  /* move to the next file, wrapping around */
  s->cur_file = (s->cur_file + 1) % s->nfiles;
  load_file(s, s->files[s->cur_file]);
}

/* ── module callbacks ─────────────────────────────── */
static void weather_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  WeatherState *s = calloc(1, sizeof(WeatherState));
  m->priv = s;

  scan_files(s);

  if (s->nfiles > 0) {
    s->cur_file = 0;
    load_file(s, s->files[0]);
  }

  /* start the first blink so we don't see a static screen instantly */
  start_blink(s);
}

static void weather_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  WeatherState *s = (WeatherState *)m->priv;

  /* card background + border */
  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, scheme[0][ColBorder].pixel);
  XDrawRectangle(dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);

  /* during blink: show nothing (just the card background) */
  if (s->transitioning)
    return;

  int pad = MODULE_PADDING;
  int line_h = drw->fonts->h;
  int max_lines = (h - 2 * pad) / line_h;
  if (max_lines > s->nlines)
    max_lines = s->nlines;

  for (int i = 0; i < max_lines; i++) {
    /* first line (city + condition) in accent colour */
    if (i == 0)
      drw_setscheme(drw, scheme[1]);
    else
      drw_setscheme(drw, scheme[0]);

    int ty = y + pad + i * line_h;
    drw_text(drw, x + pad, ty, w - 2 * pad, line_h, 0, s->text[i], 0);
  }
}

static void weather_input(Module *m, const InputEvent *ev) {
  WeatherState *s = (WeatherState *)m->priv;
  if (ev->type != EV_PRESS || ev->button != Button3)
    return;

  /* right‑click: show update time as a desktop notification */
  char updstr[64];
  if (s->last_update > 0) {
    struct tm *lt = localtime(&s->last_update);
    strftime(updstr, sizeof(updstr), "Updated: %d/%m %H:%M", lt);
  } else {
    snprintf(updstr, sizeof(updstr), "Updated: never");
  }

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "notify-send 'Weather' '%s'", updstr);
  system(cmd);
}

static void weather_timer(Module *m) {
  WeatherState *s = (WeatherState *)m->priv;

  /* if we are in a blink, check if it's time to transition to the next file */
  if (s->transitioning) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - s->trans_start.tv_sec) * 1000 +
                      (now.tv_nsec - s->trans_start.tv_nsec) / 1000000;
    if (elapsed_ms >= WEATHER_BLINK_MS) {
      finish_blink(s);
      panel_redraw(); /* show the new file */
    }
  }

  /* periodic city cycling: every WEATHER_INTERVAL seconds start a new blink */
  static time_t last_cycle = 0;
  time_t now = time(NULL);
  if (!s->transitioning && now - last_cycle >= WEATHER_INTERVAL) {
    last_cycle = now;
    start_blink(s);
    panel_redraw(); /* clear screen (blink start) */
  }
}

static void weather_destroy(Module *m) {
  WeatherState *s = (WeatherState *)m->priv;
  if (s) {
    if (s->files) {
      for (int i = 0; i < s->nfiles; i++)
        free(s->files[i]);
      free(s->files);
    }
    free(s);
  }
}

static LayoutHints *weather_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_h = 112,
      .pref_h = 112,
      .max_h = 112,
      .expand_y = 0,
      .expand_x = 1,
  };
  return &hints;
}

Module weather_module = {
    .name = "weather",
    .init = weather_init,
    .draw = weather_draw,
    .input = weather_input,
    .timer = weather_timer,
    .destroy = weather_destroy,
    .get_hints = weather_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
    .theme = (ContainerTheme *)&module_card_theme,
};

void __attribute__((constructor)) weather_register(void) {
  register_module(&weather_module);
}
