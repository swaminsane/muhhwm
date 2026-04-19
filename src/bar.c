/* bar.c - muhhwm bar drawing */

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "config.h"
#include "muhh.h"

static int bh(void) { return wm.drw->fonts->h + 2; }

void bar_init(Monitor *m) {
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"muhhwm", "muhhwm"};
  int h = bh();

  m->bar.h = h;
  m->bar.showbar = showbar;
  m->bar.topbar = topbar;

  /* position */
  m->wh = m->mh;
  if (m->bar.showbar) {
    m->wh -= h;
    m->bar.y = m->bar.topbar ? m->my : m->my + m->wh;
    if (m->bar.topbar)
      m->wy = m->my + h;
  } else {
    m->bar.y = -h;
  }
  m->bar.w = m->mw;

  if (m->bar.win)
    return;

  m->bar.win =
      XCreateWindow(wm.dpy, wm.root, m->wx, m->bar.y, m->bar.w, h, 0,
                    DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
                    DefaultVisual(wm.dpy, wm.screen),
                    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
  XDefineCursor(wm.dpy, m->bar.win, wm.cursor[CurNormal]);
  XMapRaised(wm.dpy, m->bar.win);
  XSetClassHint(wm.dpy, m->bar.win, &ch);
}

void bar_draw(Monitor *m) {
  int x, w, tw = 0;
  int boxs = wm.drw->fonts->h / 9;
  int boxw = wm.drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  int h = bh();
  Namespace *ns = &wm.ns[m->ans];
  Client *c;

  if (!m->bar.showbar)
    return;

  /* status text - only on selected monitor */
  if (m == wm.selmon) {
    drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
    tw = TEXTW(stext) - wm.lrpad + 2;
    drw_text(wm.drw, m->bar.w - tw, 0, tw, h, 0, stext, 0);
  }

  /* namespace indicator */
  drw_setscheme(wm.drw, wm.nsscheme[m->ans]);
  w = TEXTW(nsnames[m->ans]);
  drw_text(wm.drw, 0, 0, w, h, wm.lrpad / 2, nsnames[m->ans], 0);
  x = w;

  /* tags */
  for (c = ns->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  for (i = 0; i < NTAGS; i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(
        wm.drw,
        wm.scheme[ns->tagset[ns->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      drw_rect(wm.drw, x + boxs, boxs, boxw, boxw,
               m == wm.selmon && ns->sel && ns->sel->tags & 1 << i,
               urg & 1 << i);
    x += w;
  }

  /* layout symbol */
  Tag *ct = CURTAG(ns);
  w = TEXTW(ct->lt[ct->sellt]->symbol);
  drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
  x = drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, ct->lt[ct->sellt]->symbol, 0);

  /* window title */
  if ((w = m->bar.w - tw - x) > h) {
    if (ns->sel) {
      drw_setscheme(wm.drw, wm.scheme[m == wm.selmon ? SchemeSel : SchemeNorm]);
      drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, ns->sel->name, 0);
      if (ns->sel->isfloating)
        drw_rect(wm.drw, x + boxs, boxs, boxw, boxw, ns->sel->isfixed, 0);
    } else {
      drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
      drw_rect(wm.drw, x, 0, w, h, 1, 1);
    }
  }

  drw_map(wm.drw, m->bar.win, 0, 0, m->bar.w, h);
}

void bar_click(Monitor *m, int ex, int button) {
  unsigned int i, x;
  Namespace *ns = &wm.ns[m->ans];
  Arg arg = {0};
  int click = ClkRootWin;

  /* namespace label */
  x = TEXTW(nsnames[m->ans]);
  if (ex < (int)x) {
    click = ClkLtSymbol; /* reuse - namespace click */
    goto dispatch;
  }

  /* tags */
  for (i = 0; i < NTAGS; i++) {
    x += TEXTW(tags[i]);
    if (ex < (int)x) {
      click = ClkTagBar;
      arg.ui = 1 << i;
      goto dispatch;
    }
  }

  /* layout symbol */
  Tag *ct = CURTAG(ns);
  x += TEXTW(ct->lt[ct->sellt]->symbol);
  if (ex < (int)x) {
    click = ClkLtSymbol;
    goto dispatch;
  }

  /* status vs title */
  if (ex > m->bar.w - (int)TEXTW(stext) + wm.lrpad - 2)
    click = ClkStatusText;
  else
    click = ClkWinTitle;

dispatch:
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == (unsigned int)button)
      buttons[i].func(click == ClkTagBar && buttons[i].arg.ui == 0
                          ? &arg
                          : &buttons[i].arg);
}
