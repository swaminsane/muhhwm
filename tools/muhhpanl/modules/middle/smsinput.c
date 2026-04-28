#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
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

#define SMS_MAX_LEN 512

typedef struct {
  char buffer[SMS_MAX_LEN];
  int len;
  int cursor;
  int active;
  time_t last_blink;
  int cursor_visible;
} SmsState;

static SmsState *st(Module *m) { return (SmsState *)m->priv; }

static void execute_sms_command(const char *args) {
  /* runs: /home/swaminsane/.local/bin/sms <args> in background */
  if (fork() == 0) {
    setsid();
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "/home/swaminsane/.local/bin/sms %s", args);
    system(cmd);
    _exit(0);
  }
}

static void sms_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  SmsState *s = calloc(1, sizeof(SmsState));
  s->last_blink = time(NULL);
  s->cursor_visible = 1;
  m->priv = s;
}

static void sms_draw(Module *m, int x, int y, int w, int h, int focused) {
  SmsState *s = st(m);
  if (!s)
    return;

  int pad = MODULE_PADDING;
  int inner_x = x + pad, inner_y = y + pad;
  int inner_w = w - 2 * pad, inner_h = h - 2 * pad;
  int font_h = drw->fonts->h;

  /* background */
  XSetForeground(dpy, drw->gc, 0x2E3440);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  /* prompt */
  drw_setscheme(drw, scheme[0]);
  const char *prompt = "sms> ";
  int pw = drw_fontset_getwidth(drw, prompt);
  drw_text(drw, inner_x, inner_y + (inner_h - font_h) / 2, pw, font_h, 0,
           prompt, 0);

  /* editable text */
  int text_x = inner_x + pw + 2;
  int text_w = inner_w - pw - 4;
  if (text_w < 1)
    text_w = 1;

  char visible[SMS_MAX_LEN + 1];
  strncpy(visible, s->buffer, sizeof(visible) - 1);
  if (s->active && s->cursor_visible) {
    /* insert a pipe at cursor position to show where we are */
    int show_len = strlen(visible);
    char display[SMS_MAX_LEN + 4];
    strncpy(display, visible, s->cursor);
    display[s->cursor] = '\0';
    strcat(display, "│"); /* big cursor */
    strcat(display, visible + s->cursor);
    drw_text(drw, text_x, inner_y + (inner_h - font_h) / 2, text_w, font_h, 0,
             display, 0);
  } else {
    drw_text(drw, text_x, inner_y + (inner_h - font_h) / 2, text_w, font_h, 0,
             visible, 0);
  }
}

static void sms_input(Module *m, const InputEvent *ev) {
  SmsState *s = st(m);
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

  /* only process keys if we have focus and are active */
  if (ev->type != EV_KEY_PRESS || panel_get_focus() != m || !s->active)
    return;

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
  if ((ev->state & ControlMask) && ks == XK_c) { /* Ctrl+C */
    s->buffer[0] = '\0';
    s->len = 0;
    s->cursor = 0;
    s->active = 0;
    panel_set_focus(NULL);
    panel_redraw();
    return;
  }
  if (ks == XK_Return) {
    if (s->len > 0) {
      execute_sms_command(s->buffer);
      s->buffer[0] = '\0';
      s->len = 0;
      s->cursor = 0;
      panel_redraw();
    }
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
  if ((ev->state & ControlMask) && ks == XK_a) {
    s->cursor = 0;
    panel_redraw();
    return;
  }
  if ((ev->state & ControlMask) && ks == XK_e) {
    s->cursor = s->len;
    panel_redraw();
    return;
  }
  if ((ev->state & ControlMask) && ks == XK_u) {
    s->buffer[0] = '\0';
    s->len = 0;
    s->cursor = 0;
    panel_redraw();
    return;
  }
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
  if ((ev->state & ControlMask) && (ks == XK_v || ks == XK_V)) {
    FILE *p = popen(
        "xclip -selection clipboard -o 2>/dev/null || xsel -bo 2>/dev/null",
        "r");
    if (p) {
      char ch;
      while ((ch = fgetc(p)) != EOF && s->len < SMS_MAX_LEN - 1) {
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
    return;
  }
  if (len == 1 && buf[0] >= 32 && buf[0] <= 126 && s->len < SMS_MAX_LEN - 1) {
    memmove(s->buffer + s->cursor + 1, s->buffer + s->cursor,
            s->len - s->cursor + 1);
    s->buffer[s->cursor] = buf[0];
    s->cursor++;
    s->len++;
    panel_redraw();
  }
}

static void sms_timer(Module *m) {
  SmsState *s = st(m);
  if (!s || !s->active)
    return;
  if (time(NULL) - s->last_blink >= 1) {
    s->cursor_visible = !s->cursor_visible;
    s->last_blink = time(NULL);
    panel_redraw();
  }
}

static void sms_destroy(Module *m) { free(m->priv); }

static LayoutHints *sms_hints(Module *m) {
  static LayoutHints hints = {
      .min_h = 24, .pref_h = 24, .expand_x = 1, .expand_y = 0};
  return &hints;
}

Module sms_input_module = {
    .name = "smsinput",
    .init = sms_init,
    .draw = sms_draw,
    .input = sms_input,
    .timer = sms_timer,
    .destroy = sms_destroy,
    .get_hints = sms_hints,
    .margin_top = 4,
    .margin_bottom = 4,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) sms_input_register(void) {
  register_module(&sms_input_module);
}
