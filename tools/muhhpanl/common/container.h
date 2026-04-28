#ifndef CONTAINER_H
#define CONTAINER_H

#include "module.h"

extern ContainerTheme right_theme;
extern ContainerTheme module_card_theme;

/* ── module registry ─────────────────────────── */
void register_module(Module *m);
Module *module_by_name(const char *name);

/* ── manual container creation (still available) ─ */
Module *container_create(const char **names, int vertical);
Module *container_create_manual(int vertical);
Module *container_create_themed(const char **names, int vertical,
                                ContainerTheme *theme);
Module *container_create_manual_themed(int vertical, ContainerTheme *theme);
void container_add_child(Module *cont, Module *child);

/* ── layout engine ──────────────────────────── */
void container_layout(Module *root, int x, int y, int w, int h);

/* ── helpers ────────────────────────────────── */
void container_set_gap(Module *cont, int gap);
void container_set_padding(Module *cont, int pad);
void module_set_hints(Module *m, int pref_w, int pref_h, int expand_x,
                      int expand_y, float weight_x, float weight_y);
LayoutHints *module_default_hints(Module *m);
void init_themes(void);

/* keyboard focus */
void panel_set_focus(Module *m);
Module *panel_get_focus(void);

/* ═══════════════════════════════════════════════════════════════════
 *  DECLARATIVE LAYOUT TREE
 * ═══════════════════════════════════════════════════════════════════ */

enum LayoutNodeType {
  LAYOUT_MODULE,
  LAYOUT_ROW,
  LAYOUT_COL,
};

typedef struct LayoutNode LayoutNode;
struct LayoutNode {
  int type;
  const char *module_name;
  LayoutNode *children;
  int nchildren;
  float weight;
  int fixed_px;

  /* ── per‑node expand flags ── */
  int expand_x; /* 1 = allow horizontal expansion (default 1) */
  int expand_y; /* 1 = allow vertical expansion (default 1) */

  /* ── spacing fields ── */
  int gap; /* container gap override (0 = use default) */
  int margin_top, margin_bottom, margin_left, margin_right;

  ContainerTheme *theme;
};

/* build the whole panel from a tree */
Module *container_build_tree(LayoutNode *root_node);

#endif /* CONTAINER_H */
