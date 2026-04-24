#ifndef MODULE_H
#define MODULE_H

#include <X11/Xlib.h>

typedef struct ContainerTheme ContainerTheme;
typedef struct Module Module;

typedef struct {
  int min_w, min_h;
  int pref_w, pref_h;       /* desired size (0 = unset) */
  int max_w, max_h;         /* 0 = unlimited */
  int expand_x, expand_y;   /* whether extra space is taken */
  float weight_x, weight_y; /* proportional weight when expand is set */
} LayoutHints;

struct Module {
  const char *name;
  void (*init)(Module *self, int x, int y, int w, int h);
  void (*draw)(Module *self, int x, int y, int w, int h, int focused);
  void (*click)(Module *self, int x, int y, int button);
  void (*scroll)(Module *self, int x, int y, int dir);
  void (*timer)(Module *self);
  void (*motion)(Module *self, int x, int y);
  void (*key)(Module *self, int keycode, unsigned int state);
  void (*destroy)(Module *self);
  ContainerTheme *theme;
  int margin_top;
  int margin_right;
  int margin_bottom;
  int margin_left;
  LayoutHints *(*get_hints)(Module *self);
  void *priv;
  int w, h, x, y;
};

struct ContainerTheme {
  unsigned long bg, border;
  int border_w;
};

#endif
