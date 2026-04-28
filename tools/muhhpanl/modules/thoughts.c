#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
#include <stdarg.h>
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

#define MAX_ENTRIES 128
#define MAX_LINE_LEN 256

static char **entries = NULL;
static int n_entries = 0;
static time_t last_mtime = 0;
static Module *self_mod = NULL; /* for external refresh */

/* refresh data from the file */
void thoughts_refresh(void);

/* read the file and split it into entries (lines starting with '[') */
static void load_entries(void) {
  /* free old data */
  if (entries) {
    for (int i = 0; i < n_entries; i++)
      free(entries[i]);
    free(entries);
    entries = NULL;
    n_entries = 0;
  }

  struct stat st;
  if (stat(BAR_THOUGHTS_FILE, &st) != 0)
    return;
  last_mtime = st.st_mtime;

  FILE *f = fopen(BAR_THOUGHTS_FILE, "r");
  if (!f)
    return;

  char line[1024];
  char current[MAX_LINE_LEN * 4] = "";
  int cur_len = 0;

  while (fgets(line, sizeof(line), f)) {
    /* remove trailing newline */
    line[strcspn(line, "\n")] = '\0';

    /* new entry starts with '[' */
    if (line[0] == '[' && cur_len > 0) {
      entries = realloc(entries, (n_entries + 1) * sizeof(char *));
      entries[n_entries] = strdup(current);
      n_entries++;
      cur_len = 0;
      current[0] = '\0';
    }

    /* append line to current entry */
    if (line[0] == '[') {
      strcpy(current, line);
      cur_len = strlen(current);
    } else {
      if (cur_len > 0 &&
          cur_len + (int)strlen(line) + 3 < (int)sizeof(current)) {
        current[cur_len++] = ' ';
        current[cur_len] = '\0';
        strcat(current, line);
        cur_len += strlen(line);
      }
    }
  }

  /* last entry */
  if (cur_len > 0) {
    entries = realloc(entries, (n_entries + 1) * sizeof(char *));
    entries[n_entries] = strdup(current);
    n_entries++;
  }

  fclose(f);
}

void thoughts_refresh(void) {
  load_entries();
  if (self_mod)
    panel_redraw();
}

/* ── callbacks ──────────────────────────────────── */
static void thoughts_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  self_mod = m;
  load_entries();
}

static void thoughts_draw(Module *m, int x, int y, int w, int h, int focused) {
  if (m->theme) {
    XSetForeground(dpy, drw->gc, m->theme->bg);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
    if (m->theme->border_w > 0) {
      XSetForeground(dpy, drw->gc, m->theme->border);
      for (int i = 0; i < m->theme->border_w; i++)
        XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                       h - 1 - 2 * i);
    }
  }

  int pad = MODULE_PADDING;
  int inner_x = x + pad, inner_y = y + pad;
  int inner_w = w - 2 * pad, inner_h = h - 2 * pad;
  int font_h = drw->fonts->h;
  int max_lines = inner_h / font_h;
  if (max_lines < 1)
    return;

  /* show last N entries, newest at bottom */
  int start = n_entries - max_lines;
  if (start < 0)
    start = 0;

  drw_setscheme(drw, scheme[0]);
  int draw_y = inner_y;
  for (int i = start; i < n_entries; i++) {
    char visible[MAX_LINE_LEN];
    strncpy(visible, entries[i], sizeof(visible) - 1);
    int tw = drw_fontset_getwidth(drw, visible);

    /* truncate with … if too wide */
    if (tw > inner_w - 4) {
      int chop = sizeof(visible) - 4;
      while (chop > 0 && drw_fontset_getwidth(drw, visible) > inner_w - 20) {
        visible[chop] = '\0';
        chop--;
      }
      strcat(visible, "…");
    }

    drw_text(drw, inner_x + 2, draw_y, inner_w - 4, font_h, 0, visible, 0);
    draw_y += font_h;
  }
}

static void thoughts_input(Module *m, const InputEvent *ev) {
  /* no interaction for now */ (void)m;
  (void)ev;
}

static void thoughts_timer(Module *m) {
  struct stat st;
  if (stat(BAR_THOUGHTS_FILE, &st) == 0 && st.st_mtime != last_mtime) {
    load_entries();
    panel_redraw();
  }
}

static LayoutHints *thoughts_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 120, .pref_h = 200, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module thoughts_module = {
    .name = M_LEFT_THOUGHTS,
    .init = thoughts_init,
    .draw = thoughts_draw,
    .input = thoughts_input,
    .timer = thoughts_timer,
    .get_hints = thoughts_hints,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 8,
    .margin_right = 8,
    .margin_bottom = 8,
    .margin_left = 8,
};

void __attribute__((constructor)) thoughts_register(void) {
  register_module(&thoughts_module);
}
