#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
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

static const char *sink_colors[] = {COL_GREEN,  COL_CYAN,    COL_BLUE,
                                    COL_YELLOW, COL_MAGENTA, COL_RED};
static const int ncolors = sizeof(sink_colors) / sizeof(sink_colors[0]);

typedef struct {
  char **sink_names;
  int sink_count;
  int cur_sink;
} AudioState;

static void audio_refresh(AudioState *s) {
  if (s->sink_names) {
    for (int i = 0; i < s->sink_count; i++)
      free(s->sink_names[i]);
    free(s->sink_names);
    s->sink_names = NULL;
  }
  s->sink_count = 0;
  s->cur_sink = 0;

  FILE *f = popen("pactl list short sinks 2>/dev/null | awk '{print $2}'", "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0]) {
      s->sink_names =
          realloc(s->sink_names, (s->sink_count + 1) * sizeof(char *));
      s->sink_names[s->sink_count] = strdup(line);
      s->sink_count++;
    }
  }
  pclose(f);

  f = popen("pactl get-default-sink 2>/dev/null", "r");
  if (f) {
    if (fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\n")] = '\0';
      for (int i = 0; i < s->sink_count; i++) {
        if (strcmp(s->sink_names[i], line) == 0) {
          s->cur_sink = i;
          break;
        }
      }
    }
    pclose(f);
  }
}

static void audio_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  AudioState *s = calloc(1, sizeof(AudioState));
  m->priv = s;
  audio_refresh(s);
}

static void audio_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  AudioState *s = (AudioState *)m->priv;

  XSetForeground(dpy, drw->gc, scheme[0][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  const char *border_color = (s->sink_count <= 1)
                                 ? COL_BRIGHT_BLACK
                                 : sink_colors[s->cur_sink % ncolors];
  XColor bdc;
  XParseColor(dpy, DefaultColormap(dpy, screen), border_color, &bdc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bdc);
  XSetForeground(dpy, drw->gc, bdc.pixel);
  for (int i = 0; i < 4; i++)
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                   h - 1 - 2 * i);

  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, x + 4, y + 4, w - 8, h - 8);

  const char *icon = "♫";
  XftColor *fg = &scheme[0][ColFg];
  Fnt *font = drw->fonts;
  int tw = drw_fontset_getwidth(drw, icon);
  int icon_x = x + (w - tw) / 2;
  int icon_y = y + (h - font->h) / 2 + font->xfont->ascent;

  XftDraw *xftdraw =
      XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen));
  XftDrawStringUtf8(xftdraw, fg, font->xfont, icon_x, icon_y,
                    (const XftChar8 *)icon, strlen(icon));
  XftDrawDestroy(xftdraw);
}

static void audio_input(Module *m, const InputEvent *ev) {
  AudioState *s = (AudioState *)m->priv;
  if (ev->type != EV_PRESS)
    return;

  if (ev->button == Button1) {
    if (s->sink_count <= 1)
      return;
    int next = (s->cur_sink + 1) % s->sink_count;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pactl set-default-sink %s",
             s->sink_names[next]);
    system(cmd);

    struct timespec ts = {0, 200000000L};
    nanosleep(&ts, NULL);
    audio_refresh(s);
    panel_redraw();
  } else if (ev->button == Button3) {
    if (fork() == 0) {
      setsid();
      execlp("pavucontrol", "pavucontrol", NULL);
      _exit(1);
    }
  }
}

static void audio_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 5) {
    last = now;
    AudioState *s = (AudioState *)m->priv;
    audio_refresh(s);
    panel_redraw();
  }
}

static void audio_destroy(Module *m) {
  AudioState *s = (AudioState *)m->priv;
  if (s->sink_names) {
    for (int i = 0; i < s->sink_count; i++)
      free(s->sink_names[i]);
    free(s->sink_names);
  }
  free(s);
}

static LayoutHints *audio_hints(Module *m) {
  (void)m;
  static LayoutHints hints = {
      .min_w = 50,
      .pref_w = 50,
      .max_w = 50,
      .min_h = 50,
      .pref_h = 50,
      .max_h = 50,
      .expand_x = 0,
      .expand_y = 0,
  };
  return &hints;
}

Module audiosink_module = {
    .name = "audiosink",
    .init = audio_init,
    .draw = audio_draw,
    .input = audio_input,
    .timer = audio_timer,
    .destroy = audio_destroy,
    .get_hints = audio_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) audiosink_register(void) {
  register_module(&audiosink_module);
}
