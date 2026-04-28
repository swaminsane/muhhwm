#define _POSIX_C_SOURCE 200809L
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>
#include <stdarg.h>
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

static void run_cmd(const char *cmd) {
  if (fork() == 0) {
    setsid();
    system(cmd);
    _exit(0);
  }
}

static void screenshot_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
}

static void screenshot_draw(Module *m, int x, int y, int w, int h,
                            int focused) {
  /* theme background & border */
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

  /* draw ⧉ directly, no background */
  const char *icon = "⊙";
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

static void screenshot_input(Module *m, const InputEvent *ev) {
  if (ev->type != EV_PRESS)
    return;

  if (ev->button == Button1) {
    panel_hide(); /* hide panel first */
    char cmd[512];
    const char *dir = SCREENSHOT_DIR;
    snprintf(cmd, sizeof(cmd),
             "{ sleep 1; mkdir -p %s; "
             "FILE=%s/$(date +%%%%Y-%%%%m-%%%%d_%%%%H-%%%%M-%%%%S).png; "
             "scrot \"$FILE\"; "
             "notify-send \"Screenshot\" \"Saved: $(basename $FILE)\"; } &",
             dir, dir);
    run_cmd(cmd);
  } else if (ev->button == Button3) {
    run_cmd("$HOME/.local/bin/menu/scrshotmenu");
  }
}

static LayoutHints *screenshot_hints(Module *m) {
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

Module screenshot_module = {
    .name = "screenshot",
    .init = screenshot_init,
    .draw = screenshot_draw,
    .input = screenshot_input,
    .get_hints = screenshot_hints,
    .theme = (ContainerTheme *)&module_card_theme,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) screenshot_register(void) {
  register_module(&screenshot_module);
}
