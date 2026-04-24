#include "container.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Unified module registry
 * ================================================================ */
static Module **registry = NULL;
static int nmodules = 0;

void register_module(Module *m) {
  registry = realloc(registry, (nmodules + 1) * sizeof(Module *));
  registry[nmodules++] = m;
}

Module *module_by_name(const char *name) {
  for (int i = 0; i < nmodules; i++)
    if (strcmp(registry[i]->name, name) == 0)
      return registry[i];
  return NULL;
}

/* ================================================================
 *  Container implementation (unchanged except for container_create)
 * ================================================================ */
typedef struct {
  Module base;
  Module **children;
  int nchildren;
  int vertical;
} Container;

static void container_init(Module *m, int x, int y, int w, int h) {}
static void container_draw(Module *m, int x, int y, int w, int h, int f) {
  Container *c = (Container *)m;
  int off = 0;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->draw)
      continue;
    int cw = w, ch2 = h;
    if (c->vertical) {
      ch2 = ch->h ? ch->h : (h / c->nchildren);
    } else {
      cw = ch->w ? ch->w : (w / c->nchildren);
    }
    ch->draw(ch, x + (c->vertical ? 0 : off), y + (c->vertical ? off : 0), cw,
             ch2, 0);
    off += c->vertical ? ch2 : cw;
  }
}
static void container_click(Module *m, int x, int y, int b) {
  Container *c = (Container *)m;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->click)
      c->children[i]->click(c->children[i], x, y, b);
}
static void container_scroll(Module *m, int x, int y, int d) {
  Container *c = (Container *)m;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->scroll)
      c->children[i]->scroll(c->children[i], x, y, d);
}
static void container_motion(Module *m, int x, int y) {
  Container *c = (Container *)m;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->motion)
      c->children[i]->motion(c->children[i], x, y);
}
static void container_timer(Module *m) {
  Container *c = (Container *)m;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->timer)
      c->children[i]->timer(c->children[i]);
}
static void container_destroy(Module *m) {
  Container *c = (Container *)m;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->destroy)
      c->children[i]->destroy(c->children[i]);
  free(c->children);
  free(m);
}

Module *container_create(const char **names, int x, int y, int w, int h,
                         int vertical) {
  Container *c = calloc(1, sizeof(Container));
  c->base.name = "container";
  c->base.init = container_init;
  c->base.draw = container_draw;
  c->base.click = container_click;
  c->base.scroll = container_scroll;
  c->base.timer = container_timer;
  c->base.motion = container_motion;
  c->base.destroy = container_destroy;
  c->vertical = vertical;

  int n = 0;
  while (names && names[n])
    n++;
  c->children = calloc(n, sizeof(Module *));

  for (int i = 0; i < n; i++) {
    Module *mod = module_by_name(names[i]);
    if (mod)
      c->children[c->nchildren++] = mod;
    else
      fprintf(stderr, "muhhpanel: module '%s' not found\n", names[i]);
  }
  return (Module *)c;
}

Module *container_create_manual(int vertical) {
  Container *c = calloc(1, sizeof(Container));
  c->base.name = "container";
  c->base.init = container_init;
  c->base.draw = container_draw;
  c->base.click = container_click;
  c->base.scroll = container_scroll;
  c->base.timer = container_timer;
  c->base.motion = container_motion;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  return (Module *)c;
}

void container_add_child(Module *cont, Module *child) {
  Container *c = (Container *)cont;
  c->children = realloc(c->children, (c->nchildren + 1) * sizeof(Module *));
  c->children[c->nchildren++] = child;
}
