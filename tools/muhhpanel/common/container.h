#ifndef CONTAINER_H
#define CONTAINER_H

#include "module.h"

void register_module(Module *m);
Module *module_by_name(const char *name);

Module *container_create(const char **names, int vertical);
Module *container_create_manual(int vertical);
void container_add_child(Module *cont, Module *child);

/* recalculate children positions and sizes */
void container_layout(Module *self, int x, int y, int w, int h);

#endif
