#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "container.h"
#include "drw.h"
#include "input.h"
#include "module.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"

extern void thoughts_refresh(void);

#define INPUT_MAX_LEN 4096
#define MAX_LINES 6

typedef struct {
  char buffer[INPUT_MAX_LEN];
  int len;
  int cursor;
  int active;
  time_t last_blink;
  int cursor_visible;
  int wrap_offsets[MAX_LINES + 1];
  int n_wrap_lines;
  int cursor_line, cursor_x;
} InputState;

static InputState *st(Module *m) { return (InputState *)m->priv; }

/* ── helpers ─────────────────────────────────────── */
static void save_note(InputState *s) {
  if (s->len == 0)
    return;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char ts[16];
  strftime(ts, sizeof(ts), "%H:%M", tm);

  FILE *f = fopen(BAR_THOUGHTS_FILE, "a+"); /* a+ = append + read */
  if (!f)
    return;

  /* check if the last line already contains today's date */
  char date[16];
  strftime(date, sizeof(date), "%Y-%m-%d", tm);

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  char last_line[128] = "";
  if (fsize > 0) {
    /* read the last line */
    fseek(f, fsize - 120, SEEK_SET); /* go back ~120 bytes */
    char buf[128];
    while (fgets(buf, sizeof(buf), f)) {
      strncpy(last_line, buf, sizeof(last_line) - 1);
    }
  }

  int need_header = 1;
  if (strstr(last_line, date)) {
    need_header = 0;
  }

  if (need_header) {
    fprintf(f, "\n## %s\n\n", date);
  }

  fprintf(f, "[%s] %s\n", ts, s->buffer);
  fclose(f);
}

static void paste_clipboard(InputState *s) {
  FILE *p = popen("xclip -selection clipboard -o 2>/dev/null || "
                  "xsel -bo 2>/dev/null",
                  "r");
  if (!p)
    return;
  char ch;
  while ((ch = fgetc(p)) != EOF && s->len < INPUT_MAX_LEN - 1) {
    if (ch == '\n')
      ch = ' ';
    if (ch >= 32) {
      memmove(s->buffer + s->cursor + 1, s->buffer + s->cursor,
              s->len - s->cursor + 1);
      s->buffer[s->cursor] = ch;
      s->cursor++;
      s->len++;
    }
  }
  pclose(p);
  panel_redraw();
}

static void recalc_wrap(InputState *s, int inner_w) {
  int p = 0, line = 0;
  s->wrap_offsets[0] = 0;
  while (p < s->len && line < MAX_LINES) {
    int end = p;
    while (end < s->len) {
      char saved = s->buffer[end + 1];
      s->buffer[end + 1] = '\0';
      int w = drw_fontset_getwidth(drw, s->buffer + p);
      s->buffer[end + 1] = saved;
      if (w > inner_w - 8)
        break;
      end++;
    }
    if (end <= p)
      end = p + 1;
    p = end;
    line++;
    s->wrap_offsets[line] = p;
  }
  s->n_wrap_lines = line;

  s->cursor_line = 0;
  s->cursor_x = 0;
  for (int i = 0; i < s->n_wrap_lines; i++) {
    if (s->cursor >= s->wrap_offsets[i] &&
        (i == s->n_wrap_lines - 1 || s->cursor < s->wrap_offsets[i + 1])) {
      s->cursor_line = i;
      char saved = s->buffer[s->cursor];
      s->buffer[s->cursor] = '\0';
      s->cursor_x = drw_fontset_getwidth(drw, s->buffer + s->wrap_offsets[i]);
      s->buffer[s->cursor] = saved;
      break;
    }
  }
}

/* ── callbacks ──────────────────────────────────── */
static void input_init(Module *m, int x, int y, int w, int h) {
  InputState *s = calloc(1, sizeof(InputState));
  s->last_blink = time(NULL);
  s->cursor_visible = 1;
  m->priv = s;
  m->w = w;
  m->h = h;
}

static void input_draw(Module *m, int x, int y, int w, int h, int focused) {
  InputState *s = st(m);
  if (!s)
    return;

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
  int inner_w = w - 2 * pad;
  int font_h = drw->fonts->h;
  int line_h = font_h + 2;

  /* prompt – accent colour when active */
  XSetForeground(dpy, drw->gc,
                 s->active ? scheme[1][ColFg].pixel : scheme[0][ColFg].pixel);
  drw_text(drw, inner_x, inner_y + 2, 16, font_h, 0, ">", 0);

  recalc_wrap(s, inner_w);
  int text_x = inner_x + 14;
  int text_y = inner_y + 2;
  for (int i = 0; i < s->n_wrap_lines; i++) {
    int start = s->wrap_offsets[i];
    int end = (i + 1 < s->n_wrap_lines) ? s->wrap_offsets[i + 1] : s->len;
    int len = end - start;
    if (len > 0) {
      char line[1024];
      memcpy(line, s->buffer + start,
             len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1);
      line[len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1] = '\0';
      XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
      drw_text(drw, text_x, text_y, inner_w - 20, font_h, 0, line, 0);
    }
    text_y += line_h;
  }

  if (s->active && s->cursor_visible && s->cursor_line < MAX_LINES) {
    int cx = text_x + s->cursor_x;
    int cy = inner_y + 2 + s->cursor_line * line_h;
    XSetForeground(dpy, drw->gc, scheme[0][ColFg].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, cx, cy, 2, font_h);
  }
}

static void input_input(Module *m, const InputEvent *ev) {
  InputState *s = st(m);
  if (!s)
    return;

  /* click activates */
  if (ev->type == EV_PRESS && ev->button == Button1) {
    s->active = 1;
    panel_set_focus(m);
    XSetInputFocus(dpy, panel_win, RevertToPointerRoot, CurrentTime);
    panel_redraw();
    return;
  }

  /* keyboard – only if WE have focus */
  if (ev->type == EV_KEY_PRESS && panel_get_focus() != m)
    return;

  if (ev->type == EV_KEY_PRESS && s->active) {
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
      panel_set_focus(NULL);
      panel_redraw();
      return;
    }
    /* Ctrl+c – close input box (same as Escape) */
    if ((ev->state & ControlMask) && ks == XK_c) {
      s->buffer[0] = '\0';
      s->len = 0;
      s->cursor = 0;
      s->active = 0;
      panel_set_focus(NULL);
      panel_redraw();
      return;
    }
    if (ks == XK_Return) {
      save_note(s);
      s->buffer[0] = '\0';
      s->len = 0;
      s->cursor = 0;
      thoughts_refresh();
      panel_redraw();
      return;
    }
    if (ks == XK_BackSpace && s->cursor > 0) {
      memmove(s->buffer + s->cursor - 1, s->buffer + s->cursor,
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

    /* Ctrl+u – clear line */
    if ((ev->state & ControlMask) && ks == XK_u) {
      s->buffer[0] = '\0';
      s->len = 0;
      s->cursor = 0;
      panel_redraw();
      return;
    }
    /* Ctrl+w – delete last word */
    if ((ev->state & ControlMask) && ks == XK_w) {
      while (s->cursor > 0 && s->buffer[s->cursor - 1] == ' ')
        s->cursor--;
      while (s->cursor > 0 && s->buffer[s->cursor - 1] != ' ')
        s->cursor--;
      s->len = s->cursor;
      s->buffer[s->len] = '\0';
      panel_redraw();
      return;
    }
    /* Ctrl+a – start of line */
    if ((ev->state & ControlMask) && ks == XK_a) {
      s->cursor = 0;
      panel_redraw();
      return;
    }
    /* Ctrl+e – end of line */
    if ((ev->state & ControlMask) && ks == XK_e) {
      s->cursor = s->len;
      panel_redraw();
      return;
    }
    /* Ctrl+l – #link */
    if ((ev->state & ControlMask) && ks == XK_l) {
      const char *tag = "#link []()";
      int tlen = strlen(tag);
      if (s->len + tlen < INPUT_MAX_LEN - 1) {
        memmove(s->buffer + s->cursor + tlen, s->buffer + s->cursor,
                s->len - s->cursor + 1);
        memcpy(s->buffer + s->cursor, tag, tlen);
        s->cursor += tlen - 3;
        s->len += tlen;
        panel_redraw();
      }
      return;
    }
    /* Ctrl+q – #todo */
    if ((ev->state & ControlMask) && ks == XK_q) {
      const char *tag = "#todo ";
      int tlen = strlen(tag);
      if (s->len + tlen < INPUT_MAX_LEN - 1) {
        memmove(s->buffer + s->cursor + tlen, s->buffer + s->cursor,
                s->len - s->cursor + 1);
        memcpy(s->buffer + s->cursor, tag, tlen);
        s->cursor += tlen;
        s->len += tlen;
        panel_redraw();
      }
      return;
    }
    /* Ctrl+i – #idea */
    if ((ev->state & ControlMask) && ks == XK_i) {
      const char *tag = "#idea ";
      int tlen = strlen(tag);
      if (s->len + tlen < INPUT_MAX_LEN - 1) {
        memmove(s->buffer + s->cursor + tlen, s->buffer + s->cursor,
                s->len - s->cursor + 1);
        memcpy(s->buffer + s->cursor, tag, tlen);
        s->cursor += tlen;
        s->len += tlen;
        panel_redraw();
      }
      return;
    }
    /* Ctrl+v – paste */
    if ((ev->state & ControlMask) && (ks == XK_v || ks == XK_V)) {
      paste_clipboard(s);
      return;
    }
    /* Ctrl+o – open in nvim */
    if ((ev->state & ControlMask) && ks == XK_o) {
      char cmd[512];
      snprintf(cmd, sizeof(cmd), "st -e nvim + %s &", BAR_THOUGHTS_FILE);
      system(cmd);
      return;
    }

    /* printable character */
    if (len == 1 && buf[0] >= 32 && buf[0] <= 126 &&
        s->len < INPUT_MAX_LEN - 1) {
      memmove(s->buffer + s->cursor + 1, s->buffer + s->cursor,
              s->len - s->cursor + 1);
      s->buffer[s->cursor] = buf[0];
      s->cursor++;
      s->len++;
      panel_redraw();
    }
  }
}

static void input_timer(Module *m) {
  InputState *s = st(m);
  if (!s)
    return;
  if (time(NULL) - s->last_blink >= 1) {
    s->cursor_visible = !s->cursor_visible;
    s->last_blink = time(NULL);
    panel_redraw();
  }
}

static void input_destroy(Module *m) {
  InputState *s = st(m);
  if (s)
    free(s);
}

static LayoutHints *input_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 100, .pref_h = 140, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module input_module = {
    .name = M_LEFT_INPUT,
    .init = input_init,
    .draw = input_draw,
    .input = input_input,
    .timer = input_timer,
    .destroy = input_destroy,
    .get_hints = input_hints,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 6,
    .margin_right = 6,
    .margin_bottom = 6,
    .margin_left = 6,
    .can_focus = 1,
};

void __attribute__((constructor)) input_register(void) {
  register_module(&input_module);
}
