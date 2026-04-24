#ifndef CONTAINER_H
#define CONTAINER_H

#include "module.h"

void register_module(Module *m);
Module *module_by_name(const char *name);

Module *container_create(const char **names, int vertical);
Module *container_create_manual(int vertical);
Module *container_create_themed(const char **names, int vertical,
                                ContainerTheme *theme);
void container_add_child(Module *cont, Module *child);

/* Layout engine – call once after building the full tree */
void container_layout(Module *root, int x, int y, int w, int h);

/* Helpers to configure containers and modules after creation */
void container_set_gap(Module *cont, int gap);
void module_set_hints(Module *m, int pref_w, int pref_h, int expand_x,
                      int expand_y, float weight_x, float weight_y);
LayoutHints *module_default_hints(Module *m);

/* Initialise theme variables (called once after X connection is up) */
void init_themes(void);

#endif
