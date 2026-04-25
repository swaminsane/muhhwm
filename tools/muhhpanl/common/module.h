#ifndef MODULE_H
#define MODULE_H

#include "drw.h"
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>

/* forward declarations */
typedef struct Module Module;
typedef struct ContainerTheme ContainerTheme;

/* ── Event types ──────────────────────────────── */
typedef enum {
  EV_PRESS,
  EV_RELEASE,
  EV_SCROLL,
  EV_MOTION,
  EV_ENTER,
  EV_LEAVE,
  EV_KEY_PRESS,
  EV_KEY_RELEASE,
} EventType;

/* ── Unified event payload ────────────────────── */
typedef struct InputEvent {
  EventType type;

  int x, y;           /* relative to module’s inner area   */
  int root_x, root_y; /* absolute panel coordinates         */

  unsigned int button;      /* Button1 … Button5                  */
  int scroll_dx, scroll_dy; /* natural direction: >0 = down/right */

  unsigned int keycode; /* X11 keycode                        */
  unsigned int state;   /* modifier bitmask                   */
} InputEvent;

/* ── Layout hints ──────────────────────────────── */
typedef struct {
  int min_w, min_h;
  int pref_w, pref_h;
  int max_w, max_h;
  int expand_x, expand_y;
  float weight_x, weight_y;
} LayoutHints;

/* ── Visual theme ─────────────────────────────── */
struct ContainerTheme {
  unsigned long bg;
  unsigned long border;
  int border_w;
};

/* ── Module structure ──────────────────────────── */
struct Module {
  const char *name;

  void (*init)(Module *self, int x, int y, int w, int h);
  void (*destroy)(Module *self);

  void (*draw)(Module *self, int x, int y, int w, int h, int focused);
  void (*input)(Module *self, const InputEvent *ev);
  void (*timer)(Module *self);

  LayoutHints *(*get_hints)(Module *self);

  ContainerTheme *theme;
  Fnt *fontset; /* Fnt is defined in drw.h, already included */

  int is_container;
  int can_focus;
  int has_window;
  Window win;

  int margin_top, margin_right, margin_bottom, margin_left;

  void *priv;
  int x, y, w, h;
};

#endif
