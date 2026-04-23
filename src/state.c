/* state.c - muhhwm core state management */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "muhh.h"

/* forward declarations */
static void state_initnamespace(Namespace *ns, const char *name);
static void state_inittag(Tag *t);

void state_init(void) {
  int i;

  /* init all namespaces */
  for (i = 0; i < NNAMESPACES; i++)
    state_initnamespace(&wm.ns[i], nsnames[i]);

  wm.ans = 0;
}

static void state_initnamespace(Namespace *ns, const char *name) {
  int i;

  strncpy(ns->name, name, NNAMELEN - 1);
  ns->name[NNAMELEN - 1] = '\0';

  /* start on tag 1 */
  ns->tagset[0] = 1;
  ns->tagset[1] = 1;
  ns->seltags = 0;

  ns->clients = NULL;
  ns->stack = NULL;
  ns->sel = NULL;

  for (i = 0; i < NTAGS; i++)
    state_inittag(&ns->tags[i]);
}

static void state_inittag(Tag *t) {
  t->mfact = mfact;     /* from config.h */
  t->nmaster = nmaster; /* from config.h */
  t->sellt = 0;
  t->lt[0] = &layouts[0]; /* from config.h */
  t->lt[1] = &layouts[1];
  t->focused = NULL;
}

void state_switchns(Monitor *m, int ns) {
  if (ns < 0 || ns >= NNAMESPACES)
    return;
  if (ns == m->ans)
    return;

  m->ans = ns;
  wm.ans = ns;
  strip_set_ns(ns);
}

void state_seltag(const Arg *arg) {
  fprintf(stderr, "state_seltag called: %u\n", arg->ui); /* debug */
  Namespace *ns = SELNS();
  unsigned int tagmask = arg->ui;
  if (!(tagmask & TAGMASK))
    return;
  /* save previous, switch current */
  ns->tagset[ns->seltags ^ 1] = ns->tagset[ns->seltags];
  ns->seltags ^= 1;
  ns->tagset[ns->seltags] = tagmask & TAGMASK;
  x11_focus(NULL);        /* ← add */
  x11_arrange(wm.selmon); /* ← add */
  bar_draw(wm.selmon);    /* ← add */
}
