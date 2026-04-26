#include "container.h"
#include "../../colors.h"
#include "input.h"
#include "panel.h"
#include "panel_globals.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  Module registry
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
 *  Container (private)
 * ================================================================ */
typedef struct {
  Module base; /* is_container = 1 */
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

/* ================================================================
 *  Colour allocator and theme init
 * ================================================================ */
ContainerTheme right_theme;
ContainerTheme module_card_theme;

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
 *  Default hints – guaranteed minimum sizes for everything
 * ================================================================ */
static LayoutHints default_hints = {.min_h = 20,
                                    .min_w = 20,
                                    .expand_x = 1,
                                    .expand_y = 1,
                                    .weight_x = 1.0f,
                                    .weight_y = 1.0f};

static LayoutHints *get_default_hints(Module *m) {
  (void)m;
  return &default_hints;
}

static void ensure_hints(Module *m) {
  if (!m->get_hints)
    m->get_hints = get_default_hints;
}

/* ================================================================
 *  Flexbox layout engine (with min/max enforcement)
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
      exp[i] = hints ? hints->expand_y : 0;
      weight[i] = (hints && hints->weight_y > 0) ? hints->weight_y : 1.0f;
    } else {
      if (hints && hints->pref_w > 0) {
        pref[i] = hints->pref_w;
        total_pref += pref[i];
      }
      exp[i] = hints ? hints->expand_x : 0;
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
    if (pref[i] == 0 && expand_count > 0 && total_weight > 0)
      size = (int)(available * (weight[i] / total_weight));
    if (size < 1)
      size = 1;

    /* min / max clamping */
    LayoutHints *hints = ch->get_hints ? ch->get_hints(ch) : NULL;
    if (hints) {
      int min_axis = c->vertical ? hints->min_h : hints->min_w;
      if (min_axis > 0 && size < min_axis)
        size = min_axis;
      int max_axis = c->vertical ? hints->max_h : hints->max_w;
      if (max_axis > 0 && size > max_axis)
        size = max_axis;
    }

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

    /* recurse into nested containers */
    if (ch->is_container)
      container_layout(ch, ch->x, ch->y, ch->w, ch->h);

    off += size + c->gap;
  }
  free(pref);
  free(exp);
  free(weight);
}

void container_layout(Module *self, int x, int y, int w, int h) {
  if (self->is_container) {
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
 *  Drawing (with margins)
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

    if (ch->has_window)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch_h = ch->h - ch->margin_top - ch->margin_bottom;
    if (cw < 1)
      cw = 1;
    if (ch_h < 1)
      ch_h = 1;

    ch->draw(ch, cx, cy, cw, ch_h, 0);
  }
}

static void themed_container_draw(Module *self, int x, int y, int w, int h,
                                  int focused) {
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

    if (ch->has_window)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch_h = ch->h - ch->margin_top - ch->margin_bottom;
    if (cw < 1)
      cw = 1;
    if (ch_h < 1)
      ch_h = 1;

    ch->draw(ch, cx, cy, cw, ch_h, 0);
  }
}

/* ================================================================
 *  Unified input forwarding (hit‑tested, with margins)
 * ================================================================ */
static void container_input(Module *self, const InputEvent *ev) {
  Container *c = (Container *)self;

  /* broadcast motion AND key events to all children (hover + keyboard) */
  if (ev->type == EV_MOTION || ev->type == EV_KEY_PRESS) {
    for (int i = 0; i < c->nchildren; i++) {
      Module *ch = c->children[i];
      if (!ch->input)
        continue;

      InputEvent local = *ev;
      if (ev->type == EV_KEY_PRESS) {
        /* keyboard events don't need coordinates */
        local.x = 0;
        local.y = 0;
        local.root_x = ev->root_x; /* keep original for consistency */
        local.root_y = ev->root_y;
      } else {
        /* motion – convert screen coords to child‑relative */
        local.root_x = ev->root_x - panel_x;
        local.root_y = ev->root_y - panel_y;
        local.x = ev->root_x - (ch->x + panel_x + ch->margin_left);
        local.y = ev->root_y - (ch->y + panel_y + ch->margin_top);
      }
      ch->input(ch, &local);
    }
    return;
  }

  /* all other events (mouse clicks, scroll) – hit‑test using panel‑relative
   * coords */
  int px = ev->root_x - panel_x;
  int py = ev->root_y - panel_y;

  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (!ch->input)
      continue;

    int cx = ch->x + ch->margin_left;
    int cy = ch->y + ch->margin_top;
    int cw = ch->w - ch->margin_left - ch->margin_right;
    int ch_h = ch->h - ch->margin_top - ch->margin_bottom;

    if (px >= cx && px < cx + cw && py >= cy && py < cy + ch_h) {
      InputEvent local = *ev;
      local.root_x = px;
      local.root_y = py;
      local.x = px - cx;
      local.y = py - cy;
      ch->input(ch, &local);
      return;
    }
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

/* ================================================================
 *  Destroy (recursively frees hints and children)
 * ================================================================ */
static void container_destroy(Module *self) {
  Container *c = (Container *)self;
  for (int i = 0; i < c->nchildren; i++) {
    Module *ch = c->children[i];
    if (ch->destroy)
      ch->destroy(ch);
    else {
      if (ch->get_hints == module_default_hints && ch->priv)
        free(ch->priv);
    }
  }
  free(c->children);
  free(self);
}

// some extra shit
static Module *focused_module = NULL;

void panel_set_focus(Module *m) { focused_module = m; }

Module *panel_get_focus(void) { return focused_module; }

/* ================================================================
 *  Creation functions
 * ================================================================ */
Module *container_create(const char **names, int vertical) {
  Container *c = calloc(1, sizeof(Container));
  c->base.name = "container";
  c->base.is_container = 1;
  c->base.draw = container_draw;
  c->base.input = container_input;
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
      fprintf(stderr, "muhhpanl: module '%s' not found\n", names[i]);
  }
  ensure_hints(&c->base);
  return (Module *)c;
}

Module *container_create_manual(int vertical) {
  Container *c = calloc(1, sizeof(Container));
  c->base.name = "container";
  c->base.is_container = 1;
  c->base.draw = container_draw;
  c->base.input = container_input;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  c->gap = (vertical ? MODULE_VGAP : MODULE_HGAP);
  c->padding = 0;
  ensure_hints(&c->base);
  return (Module *)c;
}

Module *container_create_manual_themed(int vertical, ContainerTheme *theme) {
  ThemedContainer *tc = calloc(1, sizeof(ThemedContainer));
  Container *c = (Container *)tc;
  c->base.name = "container";
  c->base.is_container = 1;
  c->base.draw = themed_container_draw;
  c->base.input = container_input;
  c->base.timer = container_timer;
  c->base.destroy = container_destroy;
  c->vertical = vertical;
  c->gap = (vertical ? MODULE_VGAP : MODULE_HGAP);
  c->padding = CONTAINER_PADDING;
  tc->theme = theme;
  ensure_hints(&c->base);
  return (Module *)tc;
}

Module *container_create_themed(const char **names, int vertical,
                                ContainerTheme *theme) {
  ThemedContainer *tc = calloc(1, sizeof(ThemedContainer));
  Container *c = (Container *)tc;
  c->base.name = "container";
  c->base.is_container = 1;
  c->base.draw = themed_container_draw;
  c->base.input = container_input;
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
      fprintf(stderr, "muhhpanl: module '%s' not found\n", names[i]);
  }
  ensure_hints(&c->base);
  return (Module *)tc;
}

void container_add_child(Module *cont, Module *child) {
  Container *c = (Container *)cont;
  c->children = realloc(c->children, (c->nchildren + 1) * sizeof(Module *));
  c->children[c->nchildren++] = child;
  if (child->init)
    child->init(child, 0, 0, 0, 0);
}

/* ── configuration helpers ── */
void container_set_gap(Module *cont, int gap) {
  Container *c = (Container *)cont;
  c->gap = gap;
}

void container_set_padding(Module *cont, int pad) {
  Container *c = (Container *)cont;
  c->padding = pad;
}

/* ═══════════════════════════════════════════════════════════════
 *  DECLARATIVE TREE BUILDER
q* ═══════════════════════════════════════════════════════════════ */
Module *container_build_tree(LayoutNode *node) {
  if (!node)
    return NULL;

  switch (node->type) {
  case LAYOUT_MODULE: {
    if (!node->module_name)
      return NULL;
    const char *names[] = {node->module_name, NULL};
    if (node->theme)
      return container_create_themed(names, 1, node->theme);
    else
      return container_create(names, 1);
  }

  case LAYOUT_ROW:
  case LAYOUT_COL: {
    int vertical = (node->type == LAYOUT_COL);
    Module *cont;
    if (node->theme)
      cont = container_create_manual_themed(vertical, node->theme);
    else
      cont = container_create_manual(vertical);

    /* apply gap override if set */
    if (node->gap_override > 0)
      container_set_gap(cont, node->gap_override);

    for (int i = 0; i < node->nchildren; i++) {
      LayoutNode *child_node = &node->children[i];
      Module *child = container_build_tree(child_node);
      if (child) {
        /* apply weight / fixed size from the node */
        if (child_node->fixed_px > 0) {
          if (vertical)
            module_set_hints(child, 0, child_node->fixed_px, 0, 0, 0, 0);
          else
            module_set_hints(child, child_node->fixed_px, 0, 0, 0, 0, 0);
        } else {
          float w = child_node->weight > 0 ? child_node->weight : 1.0f;
          module_set_hints(child, 0, 0, 1, 1, w, w);
        }
        container_add_child(cont, child);
      }
    }
    ensure_hints(cont);
    return cont;
  }

  default:
    return NULL;
  }
}
