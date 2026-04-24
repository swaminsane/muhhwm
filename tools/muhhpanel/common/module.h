#ifndef MODULE_H
#define MODULE_H

#include <X11/Xlib.h>

typedef struct Module Module;

struct Module {
  const char *name;
  void (*init)(Module *self, int x, int y, int w, int h);
  void (*draw)(Module *self, int x, int y, int w, int h, int focused);
  void (*click)(Module *self, int x, int y, int button);
  void (*scroll)(Module *self, int x, int y, int dir);
  void (*timer)(Module *self);
  void (*motion)(Module *self, int x, int y);
  void (*destroy)(Module *self);
  void *priv;
  int w, h;
  int x, y; /* stored absolute position */
};

#endif
