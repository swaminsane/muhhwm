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

#define BT_STATE_FILE "/tmp/muhhpanl_bt"
#define DOUBLE_CLICK_MS 400

typedef struct {
  int state;          /* 0 = off, 1 = on (no connection), 2 = connected */
  char dev_name[256]; /* connected device name (if any) */
  char last_dev[256]; /* last connected device name */
  time_t last_click_time;
  int click_pending; /* 1 = waiting for possible double‑click */
} BtState;

static char *run_getline(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return NULL;
  char *buf = malloc(256);
  if (buf && fgets(buf, 256, f)) {
    pclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return buf;
  }
  free(buf);
  pclose(f);
  return NULL;
}

static void load_last_dev(BtState *s) {
  FILE *f = fopen(BT_STATE_FILE, "r");
  if (f) {
    if (fgets(s->last_dev, sizeof(s->last_dev), f)) {
      s->last_dev[strcspn(s->last_dev, "\n")] = '\0';
    }
    fclose(f);
  }
}

static void save_last_dev(const char *dev) {
  FILE *f = fopen(BT_STATE_FILE, "w");
  if (f) {
    fprintf(f, "%s\n", dev);
    fclose(f);
  }
}

static int has_bluetoothctl(void) {
  return (system("which bluetoothctl >/dev/null 2>&1") == 0);
}

static void bt_refresh(BtState *s) {
  if (!has_bluetoothctl()) {
    s->state = 0;
    s->dev_name[0] = '\0';
    return;
  }

  char *powered = run_getline(
      "bluetoothctl show 2>/dev/null | grep 'Powered:' | awk '{print $2}'");
  if (!powered || strcmp(powered, "yes") != 0) {
    s->state = 0;
    s->dev_name[0] = '\0';
    free(powered);
    return;
  }
  free(powered);

  char *dev =
      run_getline("bluetoothctl devices Connected 2>/dev/null | head -1");
  if (dev && dev[0]) {
    s->state = 2;
    char *p = dev;
    while (*p && *p != ' ')
      p++;
    if (*p == ' ')
      p++;
    strncpy(s->dev_name, p, sizeof(s->dev_name) - 1);
    strncpy(s->last_dev, p, sizeof(s->last_dev) - 1);
    save_last_dev(p);
  } else {
    s->state = 1;
    s->dev_name[0] = '\0';
  }
  free(dev);
}

static void bt_toggle_radio(void) {
  char *powered = run_getline(
      "bluetoothctl show 2>/dev/null | grep 'Powered:' | awk '{print $2}'");
  if (powered && strcmp(powered, "yes") == 0) {
    system("bluetoothctl power off");
  } else {
    system("bluetoothctl power on");
  }
  free(powered);
}

static void bt_disconnect(void) {
  system("bluetoothctl disconnect 2>/dev/null");
}

/* silently try to connect – no notifications */
static void bt_connect_last(BtState *s) {
  if (s->last_dev[0]) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "bluetoothctl connect \"%s\" 2>/dev/null",
             s->last_dev);
    system(cmd);
  }
}

static void bt_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  BtState *s = calloc(1, sizeof(BtState));
  m->priv = s;
  load_last_dev(s);
  bt_refresh(s);
}

static void bt_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  BtState *s = (BtState *)m->priv;

  XSetForeground(dpy, drw->gc, scheme[2][ColBg].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  const char *border_color;
  if (s->state == 0)
    border_color = COL_BRIGHT_BLACK;
  else if (s->state == 1)
    border_color = COL_YELLOW;
  else
    border_color = COL_GREEN;

  XColor bdc;
  XParseColor(dpy, DefaultColormap(dpy, screen), border_color, &bdc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bdc);
  XSetForeground(dpy, drw->gc, bdc.pixel);
  for (int i = 0; i < 4; i++)
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                   h - 1 - 2 * i);

  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, x + 4, y + 4, w - 8, h - 8);

  const char *symbol;
  if (s->state == 0)
    symbol = "b";
  else if (s->state == 1)
    symbol = "B";
  else
    symbol = "β";

  XftColor *fg = &scheme[0][ColFg];
  Fnt *font = drw->fonts;
  int tw = drw_fontset_getwidth(drw, symbol);
  int icon_x = x + (w - tw) / 2;
  int icon_y = y + (h - font->h) / 2 + font->xfont->ascent;

  XftDraw *xftdraw =
      XftDrawCreate(dpy, drw->drawable, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen));
  XftDrawStringUtf8(xftdraw, fg, font->xfont, icon_x, icon_y,
                    (const XftChar8 *)symbol, strlen(symbol));
  XftDrawDestroy(xftdraw);
}

static void bt_input(Module *m, const InputEvent *ev) {
  BtState *s = (BtState *)m->priv;

  if (ev->type == EV_PRESS && ev->button == Button1) {
    time_t now = time(NULL);
    if (s->click_pending &&
        difftime(now, s->last_click_time) * 1000 < DOUBLE_CLICK_MS) {
      /* double‑click → toggle radio on/off */
      bt_toggle_radio();
      s->click_pending = 0;
      bt_refresh(s);
      /* if radio just turned on, we let auto‑connect do its job – no manual
       * attempt */
      panel_redraw();
    } else {
      s->last_click_time = now;
      s->click_pending = 1;
    }
    return;
  }

  if (ev->type == EV_PRESS && ev->button == Button2) {
    bt_refresh(s);
    if (s->state == 2) {
      char noti[512];
      snprintf(noti, sizeof(noti), "notify-send 'Bluetooth' 'Connected to %s'",
               s->dev_name);
      system(noti);
    } else {
      system("notify-send 'Bluetooth' 'Not connected'");
    }
    panel_redraw();
    return;
  }

  if (ev->type == EV_PRESS && ev->button == Button3) {
    system("$HOME/.local/bin/menu/connectmenu --bluetooth &");
    return;
  }
}

static void bt_timer(Module *m) {
  static time_t last_refresh = 0;
  time_t now = time(NULL);

  BtState *s = (BtState *)m->priv;

  /* handle pending single click after 400 ms */
  if (s->click_pending &&
      difftime(now, s->last_click_time) * 1000 >= DOUBLE_CLICK_MS) {
    if (s->state == 2) {
      bt_disconnect();
    } else if (s->state == 1) {
      bt_connect_last(s); /* silent attempt */
    }
    /* if state == 0, single click does nothing */
    s->click_pending = 0;
    bt_refresh(s);
    panel_redraw();
  }

  if (now - last_refresh >= 5) {
    last_refresh = now;
    bt_refresh(s);
    panel_redraw();
  }
}

static void bt_destroy(Module *m) { free(m->priv); }

static LayoutHints *bt_hints(Module *m) {
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

Module bluetooth_module = {
    .name = "bluetooth",
    .init = bt_init,
    .draw = bt_draw,
    .input = bt_input,
    .timer = bt_timer,
    .destroy = bt_destroy,
    .get_hints = bt_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) bluetooth_register(void) {
  register_module(&bluetooth_module);
}
