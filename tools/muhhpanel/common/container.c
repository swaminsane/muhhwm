#include "container.h"
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
 *  Container structure
 * ================================================================ */
typedef struct {
  Module base;
  Module **children;
  int nchildren;
  int vertical; /* 1 = stack vertically, 0 = horizontally */
} Container;

/* forward declaration so `container_layout` can see it */
static void container_draw(Module *self, int x, int y, int w, int h,
                           int focused);

/* ================================================================
 *  Recursive layout pass (sets x,y,w,h for every module)
 * ================================================================ */
void container_layout(Module *self, int x, int y, int w, int h) {
  Container *c = (Container *)self;
  self->x = x;
  self->y = y;
  self->w = w;
  self->h = h;

  int off = 0;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    int cw, ch2;
    if (c->vertical) {
      cw = w;
      ch2 = ch->h ? ch->h : (h / c->nchildren);
    } else {
      cw = ch->w ? ch->w : (w / c->nchildren);
      ch2 = h;
    }

    int child_x = x + (c->vertical ? 0 : off);
    int child_y = y + (c->vertical ? off : 0);
    off += (c->vertical ? ch2 : cw);

    /* recurse into nested containers */
    if (ch->draw == container_draw) /* child is a container */
      container_layout(ch, child_x, child_y, cw, ch2);
    else {
      ch->x = child_x;
      ch->y = child_y;
      ch->w = cw;
      ch->h = ch2;
    }
  }
}

/* ================================================================
 *  Drawing: store child positions so hit‑testing works
 * ================================================================ */
static void container_draw(Module *self, int x, int y, int w, int h,
                           int focused) {
  (void)focused;
  Container *c = (Container *)self;
  int off = 0;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->draw)
      continue;

    int cw, ch2;
    if (c->vertical) {
      cw = w;
      ch2 = ch->h ? ch->h : (h / c->nchildren);
    } else {
      cw = ch->w ? ch->w : (w / c->nchildren);
      ch2 = h;
    }

    int child_x = x + (c->vertical ? 0 : off);
    int child_y = y + (c->vertical ? off : 0);

    ch->x = child_x;
    ch->y = child_y;
    ch->draw(ch, child_x, child_y, cw, ch2, 0);

    off += (c->vertical ? ch2 : cw);
  }
}

/* ================================================================
 *  Click forwarding with hit‑testing
 * ================================================================ */
static void container_click(Module *self, int x, int y, int btn) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->click)
      continue;
    if (x >= ch->x && x < ch->x + ch->w && y >= ch->y && y < ch->y + ch->h) {
      ch->click(ch, x - ch->x, y - ch->y, btn);
      return;
    }
  }
}

/* ================================================================
 *  Scroll forwarding with hit‑testing
 * ================================================================ */
static void container_scroll(Module *self, int x, int y, int dir) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->scroll)
      continue;
    if (x >= ch->x && x < ch->x + ch->w && y >= ch->y && y < ch->y + ch->h) {
      ch->scroll(ch, x - ch->x, y - ch->y, dir);
      return;
    }
  }
}

/* ================================================================
 *  Motion forwarding with hit‑testing (passes absolute x,y)
 * ================================================================ */
static void container_motion(Module *self, int x, int y) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    if (c->children[i]->motion)
      c->children[i]->motion(c->children[i], x, y);
  }
}

/* ================================================================
 *  Timer forwarding
 * ================================================================ */
static void container_timer(Module *self) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->timer)
      c->children[i]->timer(c->children[i]);
}

static void container_destroy(Module *self) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->destroy)
      c->children[i]->destroy(c->children[i]);
  free(c->children);
  free(self);
}

/* ================================================================
 *  Creation functions
 * ================================================================ */
Module *container_create(const char **names, int vertical) {
  Container *c = calloc(1, sizeof(Container));
  c->base.name = "container";
  c->base.draw = container_draw;
  c->base.click = container_click;
  c->base.scroll = container_scroll;
  c->base.motion = container_motion;
  c->base.timer = container_timer;
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
  c->base.draw = container_draw;
  c->base.click = container_click;
  c->base.scroll = container_scroll;
  c->base.motion = container_motion;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  return (Module *)c;
}

void container_add_child(Module *cont, Module *child) {
  Container *c = (Container *)cont;
  c->children = realloc(c->children, (c->nchildren + 1) * sizeof(Module *));
  c->children[c->nchildren++] = child;
}
