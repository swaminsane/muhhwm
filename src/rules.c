/* rules.c - window rule matching */

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"
#include "muhh.h"

static const char broken[] = "broken";

void rules_apply(Client *c) {
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  XClassHint ch = {NULL, NULL};

  c->isfloating = 0;
  c->tags = 0;
  c->ns = wm.ans; /* default: active namespace */

  XGetClassHint(wm.dpy, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      if (r->ns >= 0 && r->ns < NNAMESPACES)
        c->ns = r->ns;
    }
  }

  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);

  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK
                              : wm.ns[c->ns].tagset[wm.ns[c->ns].seltags];
}
