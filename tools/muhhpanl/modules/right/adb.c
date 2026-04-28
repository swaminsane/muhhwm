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

typedef struct {
  int state;         /* 0 = no device, 1 = one USB, 2 = multiple USB */
  char device[128];  /* serial of the first/only USB device */
  int connected;     /* 1 if any TCP/IP device exists */
  char tcp_addr[64]; /* e.g. "192.168.1.2:5555" */
} AdbState;

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

static char *get_device_ip(const char *serial) {
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "adb -s %s shell \"ip route | tail -n 1 | awk '{print \\$9}'\" "
           "2>/dev/null",
           serial);
  return run_getline(cmd);
}

static void adb_connect_device(AdbState *s, const char *serial) {
  char *ip = get_device_ip(serial);
  if (!ip || !*ip) {
    system("notify-send 'ADB' 'Could not get IP. Is Wi‑Fi on?'");
    free(ip);
    return;
  }

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "adb -s %s tcpip 5555", serial);
  system(cmd);

  system("notify-send 'ADB' 'Connecting over Wi‑Fi…'");
  sleep(2); /* wait for adbd restart */

  snprintf(cmd, sizeof(cmd), "adb connect %s:5555", ip);
  int ret = system(cmd);

  char check[256];
  snprintf(check, sizeof(check),
           "adb devices 2>/dev/null | grep -F '%s:5555' | grep -q 'device$'",
           ip);
  if (system(check) == 0) {
    s->connected = 1;
    snprintf(s->tcp_addr, sizeof(s->tcp_addr), "%s:5555", ip);
    char noti[512];
    snprintf(noti, sizeof(noti),
             "notify-send 'ADB' 'Connected %s:5555. Unplug USB.'", ip);
    system(noti);
  } else {
    system("notify-send 'ADB' 'Connection failed. Try again.'");
  }
  free(ip);
}

static void adb_refresh(AdbState *s) {
  s->state = 0;
  s->connected = 0;
  s->device[0] = '\0';
  s->tcp_addr[0] = '\0';

  char *list = run_getline("adb devices 2>/dev/null | grep -v 'List of "
                           "devices' | grep 'device$' | awk '{print $1}'");
  if (!list || !*list) {
    free(list);
    return;
  }

  int count = 0;
  char *tok = strtok(list, "\n");
  while (tok) {
    if (count == 0)
      strncpy(s->device, tok, sizeof(s->device) - 1);
    count++;
    tok = strtok(NULL, "\n");
  }
  free(list);

  s->state = (count == 1) ? 1 : 2;

  /* Check for an existing TCP connection, extract its address */
  char *tcp = run_getline("adb devices 2>/dev/null | grep ':5555' | grep "
                          "'device$' | head -1 | awk '{print $1}'");
  if (tcp && *tcp) {
    s->connected = 1;
    strncpy(s->tcp_addr, tcp, sizeof(s->tcp_addr) - 1);
    free(tcp);
  } else {
    free(tcp);
  }
}

static char *dmenu_choose(const char *items) {
  char *chosen = NULL;
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "echo \"%s\" | dmenu -i -l 10 -p 'Choose device:'",
           items);
  FILE *f = popen(cmd, "r");
  if (f) {
    char buf[256];
    if (fgets(buf, sizeof(buf), f)) {
      buf[strcspn(buf, "\n")] = '\0';
      chosen = strdup(buf);
    }
    pclose(f);
  }
  return chosen;
}

/* ── module callbacks ──────────────────────────────── */
static void adb_init(Module *m, int x, int y, int w, int h) {
  (void)x;
  (void)y;
  m->w = w;
  m->h = h;
  AdbState *s = calloc(1, sizeof(AdbState));
  m->priv = s;
  adb_refresh(s);
}

static void adb_draw(Module *m, int x, int y, int w, int h, int focused) {
  (void)focused;
  AdbState *s = (AdbState *)m->priv;

  unsigned long bg =
      (s->state == 0) ? BlackPixel(dpy, screen) : scheme[2][ColBg].pixel;
  XSetForeground(dpy, drw->gc, bg);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);

  const char *border_color;
  if (s->state == 0)
    border_color = COL_BRIGHT_BLACK;
  else if (s->connected)
    border_color = COL_GREEN;
  else
    border_color = COL_ACCENT;

  XColor bdc;
  XParseColor(dpy, DefaultColormap(dpy, screen), border_color, &bdc);
  XAllocColor(dpy, DefaultColormap(dpy, screen), &bdc);
  XSetForeground(dpy, drw->gc, bdc.pixel);
  for (int i = 0; i < 4; i++)
    XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                   h - 1 - 2 * i);

  XSetForeground(dpy, drw->gc, BlackPixel(dpy, screen));
  XDrawRectangle(dpy, drw->drawable, drw->gc, x + 4, y + 4, w - 8, h - 8);

  const char *icon = "ADB";
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

static void adb_input(Module *m, const InputEvent *ev) {
  if (ev->type != EV_PRESS)
    return;
  AdbState *s = (AdbState *)m->priv;

  if (ev->button == Button1) {
    /* Left click – connect */
    adb_refresh(s);

    if (s->state == 0) {
      system(
          "notify-send 'ADB' 'Plug in your phone and enable USB debugging.'");
      return;
    }

    if (s->state == 1) {
      adb_connect_device(s, s->device);
      adb_refresh(s);
      panel_redraw();
      return;
    }

    /* Multiple devices – pick one */
    char *list = run_getline("adb devices 2>/dev/null | grep 'device$' | grep "
                             "-v 'List' | awk '{print $1}'");
    if (!list)
      return;

    for (char *p = list; *p; p++)
      if (*p == '\n')
        *p = ' ';

    char *chosen = dmenu_choose(list);
    free(list);
    if (chosen) {
      adb_connect_device(s, chosen);
      free(chosen);
      adb_refresh(s);
      panel_redraw();
    }
  } else if (ev->button == Button2) {
    /* Middle click – disconnect TCP/IP connection */
    if (s->connected && s->tcp_addr[0]) {
      char cmd[256];
      snprintf(cmd, sizeof(cmd), "adb disconnect %s", s->tcp_addr);
      system(cmd);
      adb_refresh(s);
      panel_redraw();
      system("notify-send 'ADB' 'Disconnected'");
    }
  } else if (ev->button == Button3) {
    /* Right click – open shell in st if connected */
    if (s->connected && s->tcp_addr[0]) {
      if (fork() == 0) {
        setsid();
        execlp("st", "st", "-e", "adb", "-s", s->tcp_addr, "shell", NULL);
        _exit(1);
      }
    }
  }
}

static void adb_timer(Module *m) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last >= 5) {
    last = now;
    AdbState *s = (AdbState *)m->priv;
    adb_refresh(s);
    panel_redraw();
  }
}

static void adb_destroy(Module *m) { free(m->priv); }

static LayoutHints *adb_hints(Module *m) {
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

Module adb_module = {
    .name = M_ADB,
    .init = adb_init,
    .draw = adb_draw,
    .input = adb_input,
    .timer = adb_timer,
    .destroy = adb_destroy,
    .get_hints = adb_hints,
    .margin_top = 0,
    .margin_bottom = 0,
    .margin_left = 0,
    .margin_right = 0,
};

void __attribute__((constructor)) adb_register(void) {
  register_module(&adb_module);
}
