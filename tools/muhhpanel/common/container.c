#include "container.h"
#include "../colors.h"
#include "../panel.h"
#include "panel_globals.h"
#include "settings.h"
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
 *  Container structure (private to this file)
 * ================================================================ */
typedef struct {
  Module base;
  Module **children;
  int nchildren;
  int vertical; /* 1 = column, 0 = row */
  int gap;      /* px between children */
  int padding;  /* px inside border */
} Container;

/* themed container */
typedef struct {
  Container cont;
  ContainerTheme *theme;
} ThemedContainer;

/* forward declarations */
static void container_draw(Module *self, int x, int y, int w, int h,
                           int focused);
static void themed_container_draw(Module *self, int x, int y, int w, int h,
                                  int focused);
static void container_click(Module *self, int x, int y, int btn);
static void container_scroll(Module *self, int x, int y, int dir);
static void container_motion(Module *self, int x, int y);

/* ================================================================
 *  Theme variables (defined here, declared in panel.h)
 * ================================================================ */
ContainerTheme right_theme;
ContainerTheme module_card_theme;

/* allocate an X11 pixel from a "#rrggbb" string */
static unsigned long alloc_color(const char *hex) {
  XColor col;
  XParseColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), hex, &col);
  XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &col);
  return col.pixel;
}

void init_themes(void) {
  right_theme.bg = alloc_color(COL_BG);
  right_theme.border = alloc_color(COL_BORDER);
  right_theme.border_w = 1;

  module_card_theme.bg = alloc_color(COL_BRIGHT_BLACK);
  module_card_theme.border = alloc_color(COL_BORDER);
  module_card_theme.border_w = 1;
}

/* ================================================================
 *  LayoutHint helpers
 * ================================================================ */
LayoutHints *module_default_hints(Module *m) { return (LayoutHints *)m->priv; }

void module_set_hints(Module *m, int pref_w, int pref_h, int expand_x,
                      int expand_y, float weight_x, float weight_y) {
  LayoutHints *h = calloc(1, sizeof(LayoutHints));
  h->pref_w = pref_w;
  h->pref_h = pref_h;
  h->expand_x = expand_x;
  h->expand_y = expand_y;
  h->weight_x = weight_x;
  h->weight_y = weight_y;
  m->priv = h;
  m->get_hints = module_default_hints;
}

/* ================================================================
 *  Layout engine (weighted flexbox)
 * ================================================================ */
static void layout_instance(Container *c, int x, int y, int w, int h) {
  c->base.x = x;
  c->base.y = y;
  c->base.w = w;
  c->base.h = h;
  if (c->nchildren == 0)
    return;

  int inner_w = w - 2 * c->padding;
  int inner_h = h - 2 * c->padding;
  if (inner_w < 1)
    inner_w = 1;
  if (inner_h < 1)
    inner_h = 1;

  int total_gap = (c->nchildren - 1) * c->gap;
  int *pref = calloc(c->nchildren, sizeof(int));
  int *exp = calloc(c->nchildren, sizeof(int));
  float *weight = calloc(c->nchildren, sizeof(float));
  int total_pref = 0, expand_count = 0;
  float total_weight = 0.0f;

  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    LayoutHints *hints = ch->get_hints ? ch->get_hints(ch) : NULL;
    if (c->vertical) {
      if (hints && hints->pref_h > 0) {
        pref[i] = hints->pref_h;
        total_pref += pref[i];
      }
      exp[i] = hints ? hints->expand_y : 1;
      weight[i] = (hints && hints->weight_y > 0) ? hints->weight_y : 1.0f;
    } else {
      if (hints && hints->pref_w > 0) {
        pref[i] = hints->pref_w;
        total_pref += pref[i];
      }
      exp[i] = hints ? hints->expand_x : 1;
      weight[i] = (hints && hints->weight_x > 0) ? hints->weight_x : 1.0f;
    }
    if (exp[i]) {
      expand_count++;
      total_weight += weight[i];
    }
  }

  int available = (c->vertical ? inner_h : inner_w) - total_pref - total_gap;
  if (available < 0)
    available = 0;

  int off = 0;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    int size = pref[i] > 0 ? pref[i] : 1;
    if (pref[i] == 0 && expand_count > 0 && total_weight > 0) {
      size = (int)(available * (weight[i] / total_weight));
    }
    if (size < 1)
      size = 1;

    if (c->vertical) {
      ch->x = x + c->padding;
      ch->y = y + c->padding + off;
      ch->w = inner_w;
      ch->h = size;
    } else {
      ch->x = x + c->padding + off;
      ch->y = y + c->padding;
      ch->w = size;
      ch->h = inner_h;
    }

    /* recurse into containers */
    if (ch->draw == container_draw || ch->draw == themed_container_draw)
      container_layout(ch, ch->x, ch->y, ch->w, ch->h);

    off += size + c->gap;
  }
  free(pref);
  free(exp);
  free(weight);
}

void container_layout(Module *self, int x, int y, int w, int h) {
  if (self->draw == container_draw || self->draw == themed_container_draw) {
    Container *c = (Container *)self;
    layout_instance(c, x, y, w, h);
  } else {
    self->x = x;
    self->y = y;
    self->w = w;
    self->h = h;
  }
}

/* ================================================================
 *  Drawing (honours per‑module margins)
 * ================================================================ */
static void container_draw(Module *self, int x, int y, int w, int h,
                           int focused) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)focused;
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->draw)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch2 = ch->h - ch->margin_top - ch->margin_bottom;
    if (cw < 1)
      cw = 1;
    if (ch2 < 1)
      ch2 = 1;

    ch->draw(ch, cx, cy, cw, ch2, 0);
  }
}

static void themed_container_draw(Module *self, int x, int y, int w, int h,
                                  int focused) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)focused;
  ThemedContainer *tc = (ThemedContainer *)self;
  if (tc->theme) {
    XSetForeground(dpy, drw->gc, tc->theme->bg);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, h);
    if (tc->theme->border_w > 0) {
      XSetForeground(dpy, drw->gc, tc->theme->border);
      for (int i = 0; i < tc->theme->border_w; i++)
        XDrawRectangle(dpy, drw->drawable, drw->gc, x + i, y + i, w - 1 - 2 * i,
                       h - 1 - 2 * i);
    }
  }
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->draw)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch2 = ch->h - ch->margin_top - ch->margin_bottom;
    if (cw < 1)
      cw = 1;
    if (ch2 < 1)
      ch2 = 1;

    ch->draw(ch, cx, cy, cw, ch2, 0);
  }
}

/* ================================================================
 *  Input forwarders (honour per‑module margins)
 * ================================================================ */
static void container_click(Module *self, int x, int y, int btn) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->click)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch2 = ch->h - ch->margin_top - ch->margin_bottom;

    if (x >= cx && x < cx + cw && y >= cy && y < cy + ch2) {
      ch->click(ch, x - cx, y - cy, btn);
      return;
    }
  }
}

static void container_scroll(Module *self, int x, int y, int dir) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->scroll)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch2 = ch->h - ch->margin_top - ch->margin_bottom;

    if (x >= cx && x < cx + cw && y >= cy && y < cy + ch2) {
      ch->scroll(ch, x - cx, y - cy, dir);
      return;
    }
  }
}

static void container_motion(Module *self, int x, int y) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->motion)
      continue;
    /* pass absolute coordinates to every child */
    ch->motion(ch, x, y);
  }
}

static void container_key(Module *self, int keycode, unsigned int state) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++)
    if (c->children[i]->key)
      c->children[i]->key(c->children[i], keycode, state);
}

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
  c->base.key = container_key;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  c->gap = (vertical ? MODULE_VGAP : MODULE_HGAP);
  c->padding = 0;

  int n = 0;
  while (names && names[n])
    n++;
  c->children = calloc(n, sizeof(Module *));
  for (int i = 0; i < n; i++) {
    Module *mod = module_by_name(names[i]);
    if (mod) {
      c->children[c->nchildren++] = mod;
      if (mod->init)
        mod->init(mod, 0, 0, 0, 0);
    } else
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
  c->base.key = container_key;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  c->gap = (vertical ? MODULE_VGAP : MODULE_HGAP);
  c->padding = 0;
  return (Module *)c;
}

Module *container_create_themed(const char **names, int vertical,
                                ContainerTheme *theme) {
  ThemedContainer *tc = calloc(1, sizeof(ThemedContainer));
  Container *c = (Container *)tc;
  c->base.name = "container";
  c->base.draw = themed_container_draw;
  c->base.click = container_click;
  c->base.scroll = container_scroll;
  c->base.motion = container_motion;
  c->base.key = container_key;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  c->gap = MODULE_VGAP;
  c->padding = CONTAINER_PADDING;
  tc->theme = theme;

  int n = 0;
  while (names && names[n])
    n++;
  c->children = calloc(n, sizeof(Module *));
  for (int i = 0; i < n; i++) {
    Module *mod = module_by_name(names[i]);
    if (mod) {
      c->children[c->nchildren++] = mod;
      if (mod->init)
        mod->init(mod, 0, 0, 0, 0);
    } else
      fprintf(stderr, "muhhpanel: module '%s' not found\n", names[i]);
  }
  return (Module *)tc;
}

void container_add_child(Module *cont, Module *child) {
  Container *c = (Container *)cont;
  c->children = realloc(c->children, (c->nchildren + 1) * sizeof(Module *));
  c->children[c->nchildren++] = child;
  if (child->init)
    child->init(child, 0, 0, 0, 0);
}

/* ================================================================
 *  Gap setter
 * ================================================================ */
void container_set_gap(Module *cont, int gap) {
  Container *c = (Container *)cont;
  c->gap = gap;
}
