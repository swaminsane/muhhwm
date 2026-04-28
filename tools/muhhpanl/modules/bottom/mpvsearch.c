#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

extern void mpvbox_play_mode(const char *path, const char *mode,
                             const char *title);
extern void mpvbox_stop(void);
extern int mpvbox_is_playing(void);

#define BAR_BG 0x2E3440
#define BAR_BORDER 0x4C566A
#define TEXT_DIM 0x4C566A
#define UNDERLINE 0x5E81AC
#define HISTORY_MAX MPVSEARCH_HISTORY_MAX

typedef struct {
  char input[512];
  int len, cursor;
  int active;
  char current_path[512];
  char history[HISTORY_MAX][512];
  int n_history;
  int history_pos;
  time_t last_blink;
  int cursor_visible;
} State;

State *mpvsearch_global_state = NULL;

static State *st(Module *m) { return (State *)m->priv; }

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  char *e = s + strlen(s) - 1;
  while (e > s && (*e == ' ' || *e == '\t'))
    e--;
  *(e + 1) = '\0';
  return s;
}

static void hist_path(char *buf, size_t sz) {
  const char *home = getenv("HOME");
  if (!home)
    home = "/tmp";
  snprintf(buf, sz, "%s/.cache/muhhwm/mpvsearch_history", home);
}

static void load_hist(State *s) {
  char p[512];
  hist_path(p, sizeof(p));
  FILE *f = fopen(p, "r");
  if (!f)
    return;
  s->n_history = 0;
  while (s->n_history < HISTORY_MAX &&
         fgets(s->history[s->n_history], sizeof(s->history[0]), f)) {
    size_t l = strlen(s->history[s->n_history]);
    if (l && s->history[s->n_history][l - 1] == '\n')
      s->history[s->n_history][l - 1] = '\0';
    if (s->history[s->n_history][0])
      s->n_history++;
  }
  fclose(f);
}

static void save_hist(State *s) {
  char p[512], d[512];
  const char *h = getenv("HOME");
  snprintf(d, sizeof(d), "%s/.cache/muhhwm", h);
  mkdir(d, 0755);
  hist_path(p, sizeof(p));
  FILE *f = fopen(p, "w");
  if (!f)
    return;
  for (int i = 0; i < s->n_history; i++)
    fprintf(f, "%s\n", s->history[i]);
  fclose(f);
}

static void add_hist(State *s, const char *path) {
  if (!path || !path[0])
    return;
  for (int i = 0; i < s->n_history; i++)
    if (!strcmp(s->history[i], path)) {
      memmove(s->history[i], s->history[i + 1],
              (s->n_history - i - 1) * sizeof(s->history[0]));
      s->n_history--;
      break;
    }
  if (s->n_history >= HISTORY_MAX) {
    memmove(s->history[0], s->history[1],
            (HISTORY_MAX - 1) * sizeof(s->history[0]));
    s->n_history = HISTORY_MAX - 1;
  }
  memmove(s->history + 1, s->history, s->n_history * sizeof(s->history[0]));
  strncpy(s->history[0], path, sizeof(s->history[0]) - 1);
  s->n_history++;
  save_hist(s);
}

static void paste_clipboard(State *s) {
  FILE *p = popen("xclip -selection clipboard -o 2>/dev/null || "
                  "xsel -bo 2>/dev/null",
                  "r");
  if (!p)
    return;
  char ch;
  while ((ch = fgetc(p)) != EOF && s->len < (int)sizeof(s->input) - 1) {
    if (ch >= 32 || ch == '\n') {
      char c = (ch == '\n') ? ' ' : ch;
      memmove(s->input + s->cursor + 1, s->input + s->cursor,
              s->len - s->cursor + 1);
      s->input[s->cursor] = c;
      s->cursor++;
      s->len++;
    }
  }
  pclose(p);
  panel_redraw();
}

static int is_media_file(const char *name) {
  const char *ext = strrchr(name, '.');
  if (!ext)
    return 0;
  ext++;
  return (!strcasecmp(ext, "mp4") || !strcasecmp(ext, "mkv") ||
          !strcasecmp(ext, "avi") || !strcasecmp(ext, "webm") ||
          !strcasecmp(ext, "mov") || !strcasecmp(ext, "flv") ||
          !strcasecmp(ext, "mp3") || !strcasecmp(ext, "flac") ||
          !strcasecmp(ext, "ogg") || !strcasecmp(ext, "wav") ||
          !strcasecmp(ext, "m4a") || !strcasecmp(ext, "opus"));
}

static int name_compare(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

static void play_directory(State *s, const char *dir_path) {
  DIR *d = opendir(dir_path);
  if (!d)
    return;

  char **files = NULL;
  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(d))) {
    if (entry->d_name[0] == '.')
      continue;
    if (!is_media_file(entry->d_name))
      continue;
    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);
    files = realloc(files, (count + 1) * sizeof(char *));
    files[count] = strdup(full);
    count++;
  }
  closedir(d);

  if (count == 0)
    return;

  qsort(files, count, sizeof(char *), name_compare);
  mpvbox_play_mode(files[0], "video", NULL);

  for (int i = 0; i < count; i++)
    free(files[i]);
  free(files);
}

static void init_search(Module *m, int x, int y, int w, int h) {
  State *s = calloc(1, sizeof(State));
  s->last_blink = time(NULL);
  s->cursor_visible = 1;
  s->history_pos = -1;
  m->priv = s;
  m->w = w;
  m->h = h;
  mpvsearch_global_state = s;

  char d[512];
  const char *home = getenv("HOME");
  snprintf(d, sizeof(d), "%s/.cache/muhhwm", home);
  mkdir(d, 0755);
  load_hist(s);
}

static void draw_search(Module *m, int x, int y, int w, int h, int focused) {
  State *s = st(m);
  if (!s)
    return;
  int fh = drw->fonts->h, pad = 6;

  XSetForeground(dpy, drw->gc, BAR_BG);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
  XSetForeground(dpy, drw->gc, BAR_BORDER);
  XDrawLine(dpy, drw->drawable, drw->gc, x, y, x + w - 1, y);

  int tx = x + pad, ty = y + (h - fh) / 2, tw = w - 2 * pad;
  XSetForeground(dpy, drw->gc, s->active ? 0x3B4252 : BAR_BG);
  XFillRectangle(dpy, drw->drawable, drw->gc, tx, ty, tw, fh);
  XSetForeground(dpy, drw->gc, UNDERLINE);
  XDrawLine(dpy, drw->drawable, drw->gc, tx, ty + fh + 2, tx + tw - 1,
            ty + fh + 2);

  const char *disp = s->active ? s->input
                               : (mpvbox_is_playing() ? s->current_path
                                                      : "local path or URL…");
  if (s->active || mpvbox_is_playing())
    drw_setscheme(drw, scheme[0]);
  else
    XSetForeground(dpy, drw->gc, TEXT_DIM);
  drw_text(drw, tx + 2, ty, tw - 4, fh, 0, disp, 0);

  if (s->active && s->cursor_visible) {
    int cx = tx + 2 + drw_fontset_getwidth(drw, s->input + s->cursor);
    XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, cx, ty, 2, fh);
  }
}

static void input_search(Module *m, const InputEvent *ev) {
  State *s = st(m);
  if (!s)
    return;

  if (ev->type == EV_PRESS && ev->button == Button1 && ev->y >= 0 &&
      ev->y < m->h) {
    if (!s->active)
      s->active = 1;
    panel_set_focus(m);
    s->len = 0;
    s->cursor = 0;
    s->input[0] = '\0';
    s->history_pos = -1;
    XSetInputFocus(dpy, panel_win, RevertToPointerRoot, CurrentTime);
    panel_redraw();
    return;
  }

  if (ev->type == EV_KEY_PRESS && s->active && panel_get_focus() == m) {
    XKeyEvent ke;
    KeySym ks;
    char buf[32];
    int len;
    memset(&ke, 0, sizeof(ke));
    ke.type = KeyPress;
    ke.display = dpy;
    ke.window = panel_win;
    ke.root = root;
    ke.time = CurrentTime;
    ke.keycode = ev->keycode;
    ke.state = ev->state;
    ks = XLookupKeysym(&ke, 0);
    len = XLookupString(&ke, buf, sizeof(buf) - 1, &ks, NULL);
    buf[len] = '\0';

    if (ks == XK_Escape) {
      s->active = 0;
      s->input[0] = '\0';
      s->len = 0;
      s->cursor = 0;
      panel_redraw();
      return;
    }
    if ((ev->state & ControlMask) && (ks == XK_v || ks == XK_V)) {
      paste_clipboard(s);
      return;
    }
    if (ks == XK_Return) {
      if (s->len > 0) {
        char *clean = trim(s->input);
        if (*clean) {
          strncpy(s->current_path, clean, sizeof(s->current_path) - 1);
          add_hist(s, clean);
          struct stat st_buf;
          if (stat(clean, &st_buf) == 0 && S_ISDIR(st_buf.st_mode))
            play_directory(s, clean);
          else {
            if (!strstr(clean, "://")) {
              FILE *t = fopen(clean, "r");
              if (!t) {
                s->active = 0;
                panel_redraw();
                return;
              }
              fclose(t);
            }
            mpvbox_play_mode(clean, "video", NULL);
            panel_redraw();
          }
        }
      }
      s->active = 0;
      s->len = 0;
      s->cursor = 0;
      s->input[0] = '\0';
      panel_redraw();
      return;
    }
    if (ks == XK_Up) {
      if (s->history_pos < s->n_history - 1) {
        s->history_pos = (s->history_pos == -1) ? 0 : s->history_pos + 1;
        strncpy(s->input, s->history[s->history_pos], sizeof(s->input) - 1);
        s->len = strlen(s->input);
        s->cursor = s->len;
        panel_redraw();
      }
      return;
    }
    if (ks == XK_Down) {
      if (s->history_pos > 0) {
        s->history_pos--;
        strncpy(s->input, s->history[s->history_pos], sizeof(s->input) - 1);
        s->len = strlen(s->input);
        s->cursor = s->len;
        panel_redraw();
      } else if (s->history_pos == 0) {
        s->history_pos = -1;
        s->input[0] = '\0';
        s->len = 0;
        s->cursor = 0;
        panel_redraw();
      }
      return;
    }
    if (ks == XK_BackSpace && s->cursor > 0) {
      memmove(s->input + s->cursor - 1, s->input + s->cursor,
              s->len - s->cursor + 1);
      s->cursor--;
      s->len--;
      panel_redraw();
      return;
    }
    if (ks == XK_Left && s->cursor > 0) {
      s->cursor--;
      panel_redraw();
      return;
    }
    if (ks == XK_Right && s->cursor < s->len) {
      s->cursor++;
      panel_redraw();
      return;
    }

    /* unified Ctrl+F source menu – now saves to history and extracts title */
    if ((ev->state & ControlMask) && ks == XK_f) {
      FILE *pp = popen("/home/swaminsane/.local/bin/menu/mpvsourcemenu", "r");
      if (pp) {
        char url[512] = "";
        char line[512];
        if (fgets(line, sizeof(line), pp)) {
          line[strcspn(line, "\n")] = '\0';
          char *tab = strchr(line, '\t');
          char *tab2 = NULL;
          char mode[32] = "";

          if (tab) {
            size_t urllen = tab - line;
            if (urllen < sizeof(url)) {
              memcpy(url, line, urllen);
              url[urllen] = '\0';
            }
            tab2 = strchr(tab + 1, '\t');
            if (tab2) {
              size_t modelen = strlen(tab2 + 1);
              if (modelen < sizeof(mode))
                strncpy(mode, tab2 + 1, sizeof(mode) - 1);
            }
          } else {
            strncpy(url, line, sizeof(url) - 1);
          }

          if (url[0]) {
            /* extract title if present */
            char title_str[256] = "";
            if (tab) {
              char *title_start = tab + 1;
              char *title_end = tab2 ? tab2 : NULL;
              if (title_end) {
                size_t title_len = title_end - title_start - 1;
                if (title_len > 0 && title_len < sizeof(title_str)) {
                  memcpy(title_str, title_start, title_len);
                  title_str[title_len] = '\0';
                }
              } else {
                strncpy(title_str, title_start, sizeof(title_str) - 1);
              }
            }

            /* ── save to history ── */
            add_hist(s, url);

            if (s->len + (int)strlen(url) < (int)sizeof(s->input) - 1) {
              memmove(s->input + s->cursor + strlen(url), s->input + s->cursor,
                      s->len - s->cursor + 1);
              memcpy(s->input + s->cursor, url, strlen(url));
              s->cursor += strlen(url);
              s->len += strlen(url);
            }
            if (mode[0])
              mpvbox_play_mode(url, mode, title_str[0] ? title_str : NULL);
            else
              mpvbox_play_mode(url, "video", title_str[0] ? title_str : NULL);
            strncpy(s->current_path, url, sizeof(s->current_path) - 1);
            panel_redraw();
          }
        }
        pclose(pp);
        panel_redraw();
      }
      return;
    }

    /* printable character */
    if (len == 1 && buf[0] >= 32 && buf[0] <= 126 &&
        s->len < (int)sizeof(s->input) - 1) {
      memmove(s->input + s->cursor + 1, s->input + s->cursor,
              s->len - s->cursor + 1);
      s->input[s->cursor] = buf[0];
      s->cursor++;
      s->len++;
      panel_redraw();
    }
  }
}

static void timer_search(Module *m) {
  State *s = st(m);
  if (!s || !s->active)
    return;
  if (time(NULL) - s->last_blink >= 1) {
    s->cursor_visible = !s->cursor_visible;
    s->last_blink = time(NULL);
    panel_redraw();
  }
}

static void destroy_search(Module *m) {
  State *s = st(m);
  if (s) {
    save_hist(s);
    free(s);
  }
}

static LayoutHints *hints_search(Module *m) {
  static LayoutHints hints = {
      .min_h = 24, .pref_h = 24, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module mpvsearch_module = {
    .name = M_MPVSEARCH,
    .init = init_search,
    .draw = draw_search,
    .input = input_search,
    .timer = timer_search,
    .destroy = destroy_search,
    .get_hints = hints_search,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void mpvsearch_deactivate(void) {
  if (mpvsearch_global_state)
    mpvsearch_global_state->active = 0;
}

void __attribute__((constructor)) mpvsearch_register(void) {
  register_module(&mpvsearch_module);
}
