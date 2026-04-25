#ifndef MODULE_H
#define MODULE_H

#include "drw.h"
#include "input.h"
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>

typedef struct Module Module;
typedef struct ContainerTheme ContainerTheme;

typedef struct {
  int min_w, min_h;
  int pref_w, pref_h;
  int max_w, max_h;
  int expand_x, expand_y;
  float weight_x, weight_y;
} LayoutHints;

struct ContainerTheme {
  unsigned long bg;
  unsigned long border;
  int border_w;
};

struct Module {
  const char *name;

  void (*init)(Module *self, int x, int y, int w, int h);
  void (*destroy)(Module *self);

  void (*draw)(Module *self, int x, int y, int w, int h, int focused);
  void (*input)(Module *self, const InputEvent *ev);
  void (*timer)(Module *self); /* periodic timer */

  LayoutHints *(*get_hints)(Module *self);

  ContainerTheme *theme;
  Fnt *fontset;

  int is_container;
  int can_focus;
  int has_window;

  int margin_top, margin_right, margin_bottom, margin_left;

  void *priv;
  int x, y, w, h;
};

#endif
