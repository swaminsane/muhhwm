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

/* type of a node in the tree */
enum LayoutNodeType {
  LAYOUT_MODULE, /* leaf – just a module name */
  LAYOUT_ROW,    /* horizontal stack of children */
  LAYOUT_COL,    /* vertical stack of children */
};

typedef struct LayoutNode LayoutNode;
struct LayoutNode {
  int type; /* one of the enum above */

  /* for LAYOUT_MODULE */
  const char *module_name;

  /* for LAYOUT_ROW / LAYOUT_COL */
  LayoutNode *children; /* array of child nodes */
  int nchildren;        /* number of children */

  /* sizing hints in the parent's direction */
  float weight; /* flex weight (default 1.0) */
  int fixed_px; /* if >0, fixed pixel size */

  /* optional theme for this node (if it becomes a container) */
  ContainerTheme *theme;
};

/* build the whole panel from a tree */
Module *container_build_tree(LayoutNode *root_node);

#endif
