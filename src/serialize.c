/* serialize.c - muhhwm session persistence */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "muhh.h"

#define STATEFILE "/tmp/muhh_state"

void serialize_save(void) {
  FILE *f = fopen(STATEFILE, "w");
  if (!f)
    return;

  int i;
  Client *c;

  /* save active namespace per monitor */
  Monitor *m;
  for (m = wm.mons; m; m = m->next)
    fprintf(f, "M %d %d\n", m->num, m->ans);

  /* save namespace tag state */
  for (i = 0; i < NNAMESPACES; i++)
    fprintf(f, "N %d %u\n", i, wm.ns[i].tagset[wm.ns[i].seltags]);

  /* save each client */
  for (i = 0; i < NNAMESPACES; i++)
    for (c = wm.ns[i].clients; c; c = c->next)
      fprintf(f, "C %lu %d %u %d %d %d %d %d\n", c->win, c->ns, c->tags,
              c->isfloating, c->x, c->y, c->w, c->h);

  fclose(f);
}

void serialize_restore(void) {
  FILE *f = fopen(STATEFILE, "r");
  if (!f)
    return;

  char type;
  int i, num, ns, isfloating, x, y, w, h;
  unsigned int tags;
  unsigned long win;
  Client *c;

  while (fscanf(f, " %c", &type) == 1) {
    switch (type) {

    case 'M':
      /* restore active namespace per monitor */
      if (fscanf(f, " %d %d", &num, &ns) == 2) {
        Monitor *m;
        for (m = wm.mons; m; m = m->next)
          if (m->num == num && ns >= 0 && ns < NNAMESPACES)
            m->ans = ns;
      }
      break;

    case 'N':
      /* restore active tag per namespace */
      if (fscanf(f, " %d %u", &i, &tags) == 2)
        if (i >= 0 && i < NNAMESPACES && (tags & TAGMASK))
          wm.ns[i].tagset[wm.ns[i].seltags] = tags & TAGMASK;
      break;

    case 'C':
      /* restore client state */
      if (fscanf(f, " %lu %d %u %d %d %d %d %d", &win, &ns, &tags, &isfloating,
                 &x, &y, &w, &h) == 8) {
        for (i = 0; i < NNAMESPACES; i++) {
          for (c = wm.ns[i].clients; c; c = c->next) {
            if (c->win == (Window)win) {
              c->ns = ns;
              c->tags = tags & TAGMASK;
              c->isfloating = isfloating;
              if (c->isfloating)
                /* only restore geometry for floaters */
                c->x = x, c->y = y, c->w = w, c->h = h;
              goto next;
            }
          }
        }
      }
      break;
    }
  next:;
  }

  fclose(f);
}
