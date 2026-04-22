/* x11.c - muhhwm X11 interface */

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
#include <X11/keysym.h>

#include "config.h"
#include "muhh.h"

/* ── macros ──────────────────────────────────────────────────────────────── */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define BH (wm.drw->fonts->h + 2)
#define CLEANMASK(mask)                                                        \
  (mask & ~(numlockmask | LockMask) &                                          \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                               \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) *             \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))

/* ── file-scope state ────────────────────────────────────────────────────── */
static unsigned int numlockmask = 0;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static const char broken[] = "broken";
char stext[256]; /* status text, extern'd by bar.c */

/* ── event handler forward decls ─────────────────────────────────────────── */
static void buttonpress(XEvent *e);
static void clientmessage(XEvent *e);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void configure(Client *c);
static void pop(Client *c);
static void destroynotify(XEvent *e);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focusin(XEvent *e);
static void keypress(XEvent *e);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void propertynotify(XEvent *e);
static void unmapnotify(XEvent *e);

static void (*handler[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify,
};

/* ── static helper forward decls ─────────────────────────────────────────── */
static void applysizehints(Client *c, int *x, int *y, int *w, int *h,
                           int interact);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void restack(Monitor *m);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static int sendevent_proto(Client *c,
                           Atom proto); /* renamed to avoid conflict */
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void unfocus(Client *c, int setfocus);
static void updateclientlist(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatetitle(Client *c);
static void updateclass(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static Client *nexttiled(Client *c);
static Monitor *client_mon(Client *c);
static Monitor *createmon(void);
static Monitor *recttomon(int x, int y, int w, int h);
static Monitor *wintomon(Window w);
static Client *wintoclient(Window w);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static Atom getatomprop(Client *c, Atom prop);
static int updategeom(void);
static void cleanupmon(Monitor *mon);
#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info);
#endif

static int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;
  return XQueryPointer(wm.dpy, wm.root, &dummy, &dummy, x, y, &di, &di, &dui);
}

static long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(wm.dpy, w, wm.wmatom[WMStateAtom], 0L, 2L, False,
                         wm.wmatom[WMStateAtom], &real, &format, &n, &extra,
                         &p) != Success)
    return -1;
  if (n != 0 && format == 32)
    result = *(long *)p;
  XFree(p);
  return result;
}

static int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(wm.dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(wm.dpy, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

static void bar_updatestatus(void) {
  char tmp[256];
  if (!gettextprop(wm.root, XA_WM_NAME, tmp, sizeof tmp))
    strcpy(stext, "muhhwm");
  else
    strncpy(stext, tmp, sizeof stext - 1);
}
/* ── namespace helpers ───────────────────────────────────────────────────── */
static Monitor *client_mon(Client *c) {
  Monitor *m;
  for (m = wm.mons; m; m = m->next)
    if (m->ans == c->ns)
      return m;
  return wm.selmon;
}

/* ── client list helpers ─────────────────────────────────────────────────── */
static Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

static void attach(Client *c) {
  Namespace *ns = &wm.ns[c->ns];
  c->next = ns->clients;
  ns->clients = c;
}

static void detach(Client *c) {
  Client **tc;
  Namespace *ns = &wm.ns[c->ns];
  for (tc = &ns->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

static void attachstack(Client *c) {
  Namespace *ns = &wm.ns[c->ns];
  c->snext = ns->stack;
  ns->stack = c;
}

static void detachstack(Client *c) {
  Client **tc, *t;
  Namespace *ns = &wm.ns[c->ns];

  for (tc = &ns->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == ns->sel) {
    for (t = ns->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    ns->sel = t;
  }
}

static Client *wintoclient(Window w) {
  Client *c;
  int i;
  for (i = 0; i < NNAMESPACES; i++)
    for (c = wm.ns[i].clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

/* ── monitor helpers ─────────────────────────────────────────────────────── */
static Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = wm.selmon;
  int a, area = 0;

  for (m = wm.mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

static Monitor *wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if (w == wm.root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = wm.mons; m; m = m->next)
    if (w == m->bar.win)
      return m;
  if ((c = wintoclient(w)))
    return client_mon(c);
  return wm.selmon;
}

static Monitor *createmon(void) {
  Monitor *m = ecalloc(1, sizeof(Monitor));
  m->ans = 0;
  return m;
}

static void cleanupmon(Monitor *mon) {
  Monitor *m;

  if (mon == wm.mons)
    wm.mons = wm.mons->next;
  else {
    for (m = wm.mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(wm.dpy, mon->bar.win);
  XDestroyWindow(wm.dpy, mon->bar.win);
  free(mon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif

static int updategeom(void) {
  int dirty = 0;
#ifdef XINERAMA
  if (XineramaIsActive(wm.dpy)) {
    int i, j, n, nn;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(wm.dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = wm.mons; m; m = m->next, n++)
      ;
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    for (i = n; i < nn; i++) {
      for (m = wm.mons; m && m->next; m = m->next)
        ;
      if (m)
        m->next = createmon();
      else
        wm.mons = createmon();
    }
    for (i = 0, m = wm.mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
      }
    for (i = nn; i < n; i++) {
      for (m = wm.mons; m && m->next; m = m->next)
        ;
      if (m == wm.selmon)
        wm.selmon = wm.mons;
      cleanupmon(m);
    }
    free(unique);
  } else
#endif
  {
    if (!wm.mons)
      wm.mons = createmon();
    if (wm.mons->mw != wm.sw || wm.mons->mh != wm.sh) {
      dirty = 1;
      wm.mons->mw = wm.mons->ww = wm.sw;
      wm.mons->mh = wm.mons->wh = wm.sh;
    }
  }
  if (dirty) {
    wm.selmon = wm.mons;
    wm.selmon = wintomon(wm.root);
  }
  return dirty;
}

/* ── X property helpers ──────────────────────────────────────────────────── */
static Atom getatomprop(Client *c, Atom prop) {
  int format;
  unsigned long nitems, dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if (XGetWindowProperty(wm.dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
                         &da, &format, &nitems, &dl, &p) == Success &&
      p) {
    if (nitems > 0 && format == 32)
      atom = *(long *)p;
    XFree(p);
  }
  return atom;
}

/* ── client property helpers ─────────────────────────────────────────────── */
static void configure(Client *c) {
  XConfigureEvent ce;
  ce.type = ConfigureNotify;
  ce.display = wm.dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(wm.dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

static void setclientstate(Client *c, long state) {
  long data[] = {state, None};
  XChangeProperty(wm.dpy, c->win, wm.wmatom[WMStateAtom],
                  wm.wmatom[WMStateAtom], 32, PropModeReplace,
                  (unsigned char *)data, 2);
}

static int sendevent_proto(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;

  if (XGetWMProtocols(wm.dpy, c->win, &protocols, &n)) {
    while (!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wm.wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(wm.dpy, c->win, False, NoEventMask, &ev);
  }
  return exists;
}

static void setfocus(Client *c) {
  if (!c->neverfocus)
    XSetInputFocus(wm.dpy, c->win, RevertToPointerRoot, CurrentTime);
  XChangeProperty(wm.dpy, wm.root, wm.netatom[NetActiveWindow], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&c->win, 1);
  sendevent_proto(c, wm.wmatom[WMTakeFocus]);
}

static void setfullscreen(Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(wm.dpy, c->win, wm.netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace,
                    (unsigned char *)&wm.netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    Monitor *m = client_mon(c);
    resizeclient(c, m->mx, m->my, m->mw, m->mh);
    XRaiseWindow(wm.dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(wm.dpy, c->win, wm.netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    x11_arrange(client_mon(c));
  }
}

static void seturgent(Client *c, int urg) {
  XWMHints *wmh;
  c->isurgent = urg;
  if (!(wmh = XGetWMHints(wm.dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(wm.dpy, c->win, wmh);
  XFree(wmh);
}

static void updateclientlist(void) {
  Client *c;
  int i;
  XDeleteProperty(wm.dpy, wm.root, wm.netatom[NetClientList]);
  for (i = 0; i < NNAMESPACES; i++)
    for (c = wm.ns[i].clients; c; c = c->next)
      XChangeProperty(wm.dpy, wm.root, wm.netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
}

static void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(wm.dpy, c->win, &size, &msize))
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

static void updatetitle(Client *c) {
  if (!gettextprop(c->win, wm.netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0')
    strcpy(c->name, broken);
}

static void updateclass(Client *c) {
  XClassHint ch = {NULL, NULL};
  if (XGetClassHint(wm.dpy, c->win, &ch)) {
    if (ch.res_class) {
      strncpy(c->class, ch.res_class, sizeof c->class - 1);
      XFree(ch.res_class);
    }
    if (ch.res_name) {
      strncpy(c->instance, ch.res_name, sizeof c->instance - 1);
      XFree(ch.res_name);
    }
  }
}

static void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, wm.netatom[NetWMState]);
  Atom wtype = getatomprop(c, wm.netatom[NetWMWindowType]);

  if (state == wm.netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == wm.netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

static void updatewmhints(Client *c) {
  XWMHints *wmh;
  Namespace *ns = &wm.ns[c->ns];

  if ((wmh = XGetWMHints(wm.dpy, c->win))) {
    if (c == ns->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(wm.dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

/* ── resize/geometry ─────────────────────────────────────────────────────── */
static void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;
  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(wm.dpy, c->win,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(wm.dpy, False);
}

static void applysizehints(Client *c, int *x, int *y, int *w, int *h,
                           int interact) {
  int baseismin;
  Monitor *m = client_mon(c);
  Tag *ct = CURTAG(&wm.ns[c->ns]);

  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > wm.sw)
      *x = wm.sw - WIDTH(c);
    if (*y > wm.sh)
      *y = wm.sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < BH)
    *h = BH;
  if (*w < BH)
    *w = BH;
  if (resizehints || c->isfloating || !ct->lt[ct->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(c);
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) {
      *w -= c->basew;
      *h -= c->baseh;
    }
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) {
      *w -= c->basew;
      *h -= c->baseh;
    }
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
}

static void resize(Client *c, int x, int y, int w, int h, int interact) {
  int ox = c->x, oy = c->y, ow = c->w, oh = c->h;
  applysizehints(c, &x, &y, &w, &h, interact);
  if (x != ox || y != oy || w != ow || h != oh)
    resizeclient(c, x, y, w, h);
}

/* ── input handling ──────────────────────────────────────────────────────── */
static void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(wm.dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(wm.dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

static void grabbuttons(Client *c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    XUngrabButton(wm.dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(wm.dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                  GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(wm.dpy, buttons[i].button, buttons[i].mask | modifiers[j],
                      c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync,
                      None, None);
  }
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
    XDisplayKeycodes(wm.dpy, &start, &end);
    syms = XGetKeyboardMapping(wm.dpy, start, end - start + 1, &skip);
    if (!syms)
      return;
    for (k = start; k <= (unsigned int)end; k++)
      for (i = 0; i < LENGTH(keys); i++)
        if (keys[i].keysym == syms[(k - start) * skip])
          for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(wm.dpy, k, keys[i].mod | modifiers[j], wm.root, True,
                     GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

/* ── layout helpers ──────────────────────────────────────────────────────── */
static void showhide(Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    XMoveWindow(wm.dpy, c->win, c->x, c->y);
    Tag *ct = CURTAG(&wm.ns[c->ns]);
    if ((!ct->lt[ct->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    showhide(c->snext);
    XMoveWindow(wm.dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

static void unfocus(Client *c, int sf) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(wm.dpy, c->win, wm.scheme[SchemeNorm][ColBorder].pixel);
  if (sf) {
    XSetInputFocus(wm.dpy, wm.root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(wm.dpy, wm.root, wm.netatom[NetActiveWindow]);
  }
}

static void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;
  Namespace *ns = &wm.ns[m->ans];
  Tag *ct = CURTAG(ns);

  bar_draw(m);
  if (!ns->sel)
    return;
  if (ns->sel->isfloating || !ct->lt[ct->sellt]->arrange)
    XRaiseWindow(wm.dpy, ns->sel->win);
  if (ct->lt[ct->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->bar.win;
    for (c = ns->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(wm.dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(wm.dpy, False);
  while (XCheckMaskEvent(wm.dpy, EnterWindowMask, &ev))
    ;
}

static void pop(Client *c) {
  detach(c);
  attach(c);
  x11_focus(c);
  x11_arrange(client_mon(c));
}

/* ── layouts ─────────────────────────────────────────────────────────────── */
void tile(Monitor *m) {
  unsigned int i, n, h, mw, my, ty;
  Client *c;
  Namespace *ns = &wm.ns[m->ans];
  Tag *ct = CURTAG(ns);

  for (n = 0, c = nexttiled(ns->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  mw = (n > (unsigned int)ct->nmaster) ? (ct->nmaster ? m->ww * ct->mfact : 0)
                                       : m->ww;

  for (i = my = ty = 0, c = nexttiled(ns->clients); c;
       c = nexttiled(c->next), i++) {
    if (i < (unsigned int)ct->nmaster) {
      h = (m->wh - my) / (MIN(n, (unsigned int)ct->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - 2 * c->bw, h - 2 * c->bw, 0);
      if (my + HEIGHT(c) < (unsigned int)m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - 2 * c->bw, h - 2 * c->bw,
             0);
      if (ty + HEIGHT(c) < (unsigned int)m->wh)
        ty += HEIGHT(c);
    }
  }
}

void monocle(Monitor *m) {
  Client *c;
  Namespace *ns = &wm.ns[m->ans];

  for (c = nexttiled(ns->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

/* ── error handlers ──────────────────────────────────────────────────────── */
static int xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "muhhwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee);
}

static int xerrordummy(Display *dpy, XErrorEvent *ee) {
  (void)dpy;
  (void)ee;
  return 0;
}

static int xerrorstart(Display *dpy, XErrorEvent *ee) {
  (void)dpy;
  (void)ee;
  die("muhhwm: another window manager is already running");
  return -1;
}

/* ── event handlers ──────────────────────────────────────────────────────── */
static void buttonpress(XEvent *e) {
  unsigned int i, x, click;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  if ((m = wintomon(ev->window)) && m != wm.selmon) {
    unfocus(SELNS()->sel, 1);
    wm.selmon = m;
    x11_focus(NULL);
  }
  if (ev->window == wm.selmon->bar.win) {
    bar_click(wm.selmon, ev->x, ev->button);
    return;
  } else if ((c = wintoclient(ev->window))) {
    x11_focus(c);
    restack(wm.selmon);
    XAllowEvents(wm.dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(&buttons[i].arg);
  (void)x;
}

static void clientmessage(XEvent *e) {
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (!c)
    return;
  if (cme->message_type == wm.netatom[NetWMState]) {
    if (cme->data.l[1] == (long)wm.netatom[NetWMFullscreen] ||
        cme->data.l[2] == (long)wm.netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 ||
                        (cme->data.l[0] == 2 && !c->isfullscreen)));
  } else if (cme->message_type == wm.netatom[NetActiveWindow]) {
    Namespace *ns = &wm.ns[c->ns];
    if (c != ns->sel && !c->isurgent)
      seturgent(c, 1);
  }
}

static void configurenotify(XEvent *e) {
  Monitor *m;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  if (ev->window == wm.root) {
    dirty = (wm.sw != ev->width || wm.sh != ev->height);
    wm.sw = ev->width;
    wm.sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(wm.drw, wm.sw, BH);
      for (m = wm.mons; m; m = m->next)
        bar_init(m);
      x11_focus(NULL);
      x11_arrange(NULL);
    }
  }
}

static void configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating ||
             !CURTAG(SELNS())->lt[CURTAG(SELNS())->sellt]->arrange) {
      m = client_mon(c);
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2);
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2);
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(wm.dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(wm.dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(wm.dpy, False);
}

static void destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;
  if ((c = wintoclient(ev->window)))
    x11_unmanage(c, 1);
}

static void enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != wm.root)
    return;
  c = wintoclient(ev->window);
  m = c ? client_mon(c) : wintomon(ev->window);
  if (m != wm.selmon) {
    unfocus(SELNS()->sel, 1);
    wm.selmon = m;
  } else if (!c || c == SELNS()->sel)
    return;
  x11_focus(c);
}

static void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;
  if (ev->count == 0 && (m = wintomon(ev->window)))
    bar_draw(m);
}

static void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  Namespace *ns = SELNS();
  if (ns->sel && ev->window != ns->sel->win)
    setfocus(ns->sel);
}

static void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev = &e->xkey;

  keysym = XLookupKeysym(ev, (ev->state & ShiftMask) ? 1 : 0);

  if (bar_whichkey_active()) {
    bar_whichkey_key(keysym);
    return;
  }

  if (keysym == XK_x && CLEANMASK(ev->state) == CLEANMASK(MODKEY)) {
    bar_whichkey_activate();
    return;
  }

  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

static void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;
  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

static void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  if (!XGetWindowAttributes(wm.dpy, ev->window, &wa) || wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    x11_manage(ev->window, &wa);
}

static void motionnotify(XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != wm.root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(SELNS()->sel, 1);
    wm.selmon = m;
    x11_focus(NULL);
  }
  mon = m;
}

static void propertynotify(XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((ev->window == wm.root) && (ev->atom == XA_WM_NAME)) {
    if (!gettextprop(wm.root, XA_WM_NAME, stext, sizeof stext))
      strcpy(stext, "muhhwm");
    bar_draw(wm.selmon);
  } else if (ev->state == PropertyDelete) {
    return;
  } else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(wm.dpy, c->win, &trans)) &&
          (c->isfloating = (wintoclient(trans)) != NULL))
        x11_arrange(client_mon(c));
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      bar_draw(wm.selmon);
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == wm.netatom[NetWMName]) {
      updatetitle(c);
      if (ev->atom == XA_WM_NAME || ev->atom == wm.netatom[NetWMName]) {
        updatetitle(c);
        activity_update_title(c); /* ← add this */
        if (c == wm.ns[c->ns].sel)
          bar_draw(client_mon(c));
      }
      if (c == wm.ns[c->ns].sel)
        bar_draw(client_mon(c));
    }
    if (ev->atom == wm.netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

static void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;
  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      x11_unmanage(c, 0);
  }
}

/* ── exported: core ──────────────────────────────────────────────────────── */
void x11_focus(Client *c) {
  Namespace *ns = SELNS();

  if (!c || !ISVISIBLE(c))
    for (c = ns->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (ns->sel && ns->sel != c)
    unfocus(ns->sel, 0);
  if (c) {
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(wm.dpy, c->win, wm.scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(wm.dpy, wm.root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(wm.dpy, wm.root, wm.netatom[NetActiveWindow]);
  }
  ns->sel = c;
  activity_focus(c);
  Monitor *m;
  for (m = wm.mons; m; m = m->next)
    bar_draw(m);
}
void x11_arrange(Monitor *m) {
  Namespace *ns;
  Tag *ct;

  if (m) {
    ns = &wm.ns[m->ans];
    showhide(ns->stack);
    ct = CURTAG(ns);
    if (ct->lt[ct->sellt]->arrange)
      ct->lt[ct->sellt]->arrange(m);
    restack(m);
  } else {
    for (m = wm.mons; m; m = m->next) {
      ns = &wm.ns[m->ans];
      showhide(ns->stack);
    }
    for (m = wm.mons; m; m = m->next) {
      ns = &wm.ns[m->ans];
      ct = CURTAG(ns);
      if (ct->lt[ct->sellt]->arrange)
        ct->lt[ct->sellt]->arrange(m);
    }
  }
}

void x11_manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;
  Monitor *m = wm.selmon;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;
  c->ns = wm.ans;

  updatetitle(c);
  updateclass(c);
  if (XGetTransientForHint(wm.dpy, w, &trans) && (t = wintoclient(trans))) {
    c->ns = t->ns;
    c->tags = t->tags;
  } else {
    rules_apply(c);
  }

  if (c->x + WIDTH(c) > m->wx + m->ww)
    c->x = m->wx + m->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > m->wy + m->wh)
    c->y = m->wy + m->wh - HEIGHT(c);
  c->x = MAX(c->x, m->wx);
  c->y = MAX(c->y, m->wy);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(wm.dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(wm.dpy, w, wm.scheme[SchemeNorm][ColBorder].pixel);
  configure(c);
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(wm.dpy, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(wm.dpy, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(wm.dpy, wm.root, wm.netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(wm.dpy, c->win, c->x + 2 * wm.sw, c->y, c->w, c->h);
  setclientstate(c, NormalState);
  if (c->ns == wm.selmon->ans)
    unfocus(wm.ns[c->ns].sel, 0);
  wm.ns[c->ns].sel = c;
  x11_arrange(m);
  XMapWindow(wm.dpy, c->win);
  x11_focus(NULL);
}

void x11_unmanage(Client *c, int destroyed) {
  Monitor *m = client_mon(c);
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(wm.dpy);
    XSetErrorHandler(xerrordummy);
    XSelectInput(wm.dpy, c->win, NoEventMask);
    XConfigureWindow(wm.dpy, c->win, CWBorderWidth, &wc);
    XUngrabButton(wm.dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(wm.dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(wm.dpy);
  }
  free(c);
  x11_focus(NULL);
  updateclientlist();
  x11_arrange(m);
}

void x11_init(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  Monitor *m;

  wm.screen = DefaultScreen(wm.dpy);
  wm.sw = DisplayWidth(wm.dpy, wm.screen);
  wm.sh = DisplayHeight(wm.dpy, wm.screen);
  wm.root = RootWindow(wm.dpy, wm.screen);
  wm.drw = drw_create(wm.dpy, wm.screen, wm.root, wm.sw, wm.sh);
  if (!drw_fontset_create(wm.drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  wm.lrpad = wm.drw->fonts->h;

  xerrorxlib = XSetErrorHandler(xerrorstart);
  XSelectInput(wm.dpy, DefaultRootWindow(wm.dpy), SubstructureRedirectMask);
  XSync(wm.dpy, False);
  XSetErrorHandler(xerror);
  XSync(wm.dpy, False);

  updategeom();

  utf8string = XInternAtom(wm.dpy, "UTF8_STRING", False);
  wm.wmatom[WMProtocols] = XInternAtom(wm.dpy, "WM_PROTOCOLS", False);
  wm.wmatom[WMDelete] = XInternAtom(wm.dpy, "WM_DELETE_WINDOW", False);
  wm.wmatom[WMStateAtom] = XInternAtom(wm.dpy, "WM_STATE", False);
  wm.wmatom[WMTakeFocus] = XInternAtom(wm.dpy, "WM_TAKE_FOCUS", False);
  wm.netatom[NetActiveWindow] =
      XInternAtom(wm.dpy, "_NET_ACTIVE_WINDOW", False);
  wm.netatom[NetSupported] = XInternAtom(wm.dpy, "_NET_SUPPORTED", False);
  wm.netatom[NetWMName] = XInternAtom(wm.dpy, "_NET_WM_NAME", False);
  wm.netatom[NetWMState] = XInternAtom(wm.dpy, "_NET_WM_STATE", False);
  wm.netatom[NetWMCheck] =
      XInternAtom(wm.dpy, "_NET_SUPPORTING_WM_CHECK", False);
  wm.netatom[NetWMFullscreen] =
      XInternAtom(wm.dpy, "_NET_WM_STATE_FULLSCREEN", False);
  wm.netatom[NetWMWindowType] =
      XInternAtom(wm.dpy, "_NET_WM_WINDOW_TYPE", False);
  wm.netatom[NetWMWindowTypeDialog] =
      XInternAtom(wm.dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  wm.netatom[NetClientList] = XInternAtom(wm.dpy, "_NET_CLIENT_LIST", False);

  {
    Cur *cur;
    cur = drw_cur_create(wm.drw, XC_left_ptr);
    wm.cursor[CurNormal] = cur->cursor;
    free(cur);
    cur = drw_cur_create(wm.drw, XC_sizing);
    wm.cursor[CurResize] = cur->cursor;
    free(cur);
    cur = drw_cur_create(wm.drw, XC_fleur);
    wm.cursor[CurMove] = cur->cursor;
    free(cur);
  }

  wm.scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    wm.scheme[i] = drw_scm_create(wm.drw, colors[i], 3);

  for (i = 0; i < NNAMESPACES; i++)
    wm.nsscheme[i] = drw_scm_create(wm.drw, nscolors[i], 3);

  for (m = wm.mons; m; m = m->next)
    bar_init(m);

  strcpy(stext, "muhhwm");

  wm.wmcheckwin = XCreateSimpleWindow(wm.dpy, wm.root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(wm.dpy, wm.wmcheckwin, wm.netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm.wmcheckwin, 1);
  XChangeProperty(wm.dpy, wm.wmcheckwin, wm.netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"muhhwm", 6);
  XChangeProperty(wm.dpy, wm.root, wm.netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm.wmcheckwin, 1);
  XChangeProperty(wm.dpy, wm.root, wm.netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)wm.netatom, NetLast);
  XDeleteProperty(wm.dpy, wm.root, wm.netatom[NetClientList]);

  wa.cursor = wm.cursor[CurNormal];
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(wm.dpy, wm.root, CWEventMask | CWCursor, &wa);
  XSelectInput(wm.dpy, wm.root, wa.event_mask);
  grabkeys();
  wm.running = 1;
  x11_focus(NULL);
}

void x11_scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(wm.dpy, wm.root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(wm.dpy, wins[i], &wa) || wa.override_redirect ||
          XGetTransientForHint(wm.dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        x11_manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(wm.dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(wm.dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        x11_manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void x11_run(void) {
  XEvent ev;
  XSync(wm.dpy, False);
  while (wm.running && !XNextEvent(wm.dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev);
}

/* ── exported: key handlers ──────────────────────────────────────────────── */
void focusstack(const Arg *arg) {
  Client *c = NULL, *i;
  Namespace *ns = SELNS();

  if (!ns->sel)
    return;
  if (arg->i > 0) {
    for (c = ns->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = ns->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = ns->clients; i != ns->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    x11_focus(c);
    restack(wm.selmon);
  }
}

void incnmaster(const Arg *arg) {
  Tag *ct = CURTAG(SELNS());
  ct->nmaster = MAX(ct->nmaster + arg->i, 0);
  x11_arrange(wm.selmon);
}

void killclient(const Arg *arg) {
  Namespace *ns = SELNS();
  (void)arg;
  if (!ns->sel)
    return;
  if (!sendevent_proto(ns->sel, wm.wmatom[WMDelete])) {
    XGrabServer(wm.dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(wm.dpy, DestroyAll);
    XKillClient(wm.dpy, ns->sel->win);
    XSync(wm.dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(wm.dpy);
  }
}

void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;
  (void)arg;

  if (!(c = SELNS()->sel))
    return;
  if (c->isfullscreen)
    return;
  restack(wm.selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(wm.dpy, wm.root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, wm.cursor[CurMove],
                   CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(wm.dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
        continue;
      lasttime = ev.xmotion.time;
      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(wm.selmon->wx - nx) < snap)
        nx = wm.selmon->wx;
      else if (abs((wm.selmon->wx + wm.selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = wm.selmon->wx + wm.selmon->ww - WIDTH(c);
      if (abs(wm.selmon->wy - ny) < snap)
        ny = wm.selmon->wy;
      else if (abs((wm.selmon->wy + wm.selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = wm.selmon->wy + wm.selmon->wh - HEIGHT(c);
      if (!c->isfloating &&
          CURTAG(SELNS())->lt[CURTAG(SELNS())->sellt]->arrange &&
          (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!CURTAG(SELNS())->lt[CURTAG(SELNS())->sellt]->arrange ||
          c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(wm.dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != wm.selmon) {
    wm.selmon = m;
    x11_focus(NULL);
  }
}

void quit(const Arg *arg) {
  (void)arg;
  wm.running = 0;
}

void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;
  (void)arg;

  if (!(c = SELNS()->sel))
    return;
  if (c->isfullscreen)
    return;
  restack(wm.selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(wm.dpy, wm.root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, wm.cursor[CurResize],
                   CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(wm.dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  do {
    XMaskEvent(wm.dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
        continue;
      lasttime = ev.xmotion.time;
      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (!c->isfloating &&
          CURTAG(SELNS())->lt[CURTAG(SELNS())->sellt]->arrange &&
          (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
        togglefloating(NULL);
      if (!CURTAG(SELNS())->lt[CURTAG(SELNS())->sellt]->arrange ||
          c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(wm.dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  XUngrabPointer(wm.dpy, CurrentTime);
  while (XCheckMaskEvent(wm.dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != wm.selmon) {
    wm.selmon = m;
    x11_focus(NULL);
  }
}

void setlayout(const Arg *arg) {
  Tag *ct = CURTAG(SELNS());
  if (!arg || !arg->v || arg->v != ct->lt[ct->sellt])
    ct->sellt ^= 1;
  if (arg && arg->v)
    ct->lt[ct->sellt] = (Layout *)arg->v;
  if (SELNS()->sel)
    x11_arrange(wm.selmon);
  else
    bar_draw(wm.selmon);
}

void setmfact(const Arg *arg) {
  float f;
  Tag *ct = CURTAG(SELNS());

  if (!arg || !ct->lt[ct->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + ct->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  ct->mfact = f;
  x11_arrange(wm.selmon);
}

void spawn(const Arg *arg) {
  struct sigaction sa;
  if (fork() == 0) {
    if (wm.dpy)
      close(ConnectionNumber(wm.dpy));
    setsid();
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);
    const char *home = getenv("HOME");
    if (home)
      chdir(home);
    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("muhhwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}

void tag(const Arg *arg) {
  Namespace *ns = SELNS();
  if (ns->sel && arg->ui & TAGMASK) {
    ns->sel->tags = arg->ui & TAGMASK;
    x11_focus(NULL);
    x11_arrange(wm.selmon);
  }
}

void togglebar(const Arg *arg) {
  int h = BH;
  (void)arg;
  wm.selmon->bar.showbar = !wm.selmon->bar.showbar;
  wm.selmon->wy = wm.selmon->my;
  wm.selmon->wh = wm.selmon->mh;
  if (wm.selmon->bar.showbar) {
    wm.selmon->wh -= h;
    wm.selmon->bar.y =
        wm.selmon->bar.topbar ? wm.selmon->my : wm.selmon->my + wm.selmon->wh;
    if (wm.selmon->bar.topbar)
      wm.selmon->wy = wm.selmon->my + h;
  } else {
    wm.selmon->bar.y = -h;
  }
  XMoveResizeWindow(wm.dpy, wm.selmon->bar.win, wm.selmon->wx, wm.selmon->bar.y,
                    wm.selmon->bar.w, h);
  x11_arrange(wm.selmon);
}

void togglefloating(const Arg *arg) {
  Namespace *ns = SELNS();
  (void)arg;
  if (!ns->sel || ns->sel->isfullscreen)
    return;
  ns->sel->isfloating = !ns->sel->isfloating || ns->sel->isfixed;
  if (ns->sel->isfloating)
    resize(ns->sel, ns->sel->x, ns->sel->y, ns->sel->w, ns->sel->h, 0);
  x11_arrange(wm.selmon);
}

void toggletag(const Arg *arg) {
  unsigned int newtags;
  Namespace *ns = SELNS();

  if (!ns->sel)
    return;
  newtags = ns->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    ns->sel->tags = newtags;
    x11_focus(NULL);
    x11_arrange(wm.selmon);
  }
}

void zoom(const Arg *arg) {
  Namespace *ns = SELNS();
  Tag *ct = CURTAG(ns);
  Client *c = ns->sel;
  (void)arg;

  if (!ct->lt[ct->sellt]->arrange || !c || c->isfloating)
    return;
  if (c == nexttiled(ns->clients) && !(c = nexttiled(c->next)))
    return;
  pop(c);
}

void switchns(const Arg *arg) {
  int ns = arg->i;
  Client *c;
  int i;

  if (ns < 0 || ns >= NNAMESPACES || ns == wm.selmon->ans)
    return;

  /* hide all clients from all namespaces */
  for (i = 0; i < NNAMESPACES; i++)
    for (c = wm.ns[i].clients; c; c = c->next)
      XMoveWindow(wm.dpy, c->win, WIDTH(c) * -2, c->y);

  state_switchns(wm.selmon, ns);
  x11_arrange(wm.selmon);
  x11_focus(NULL);
  bar_draw(wm.selmon);
}

/* ── adjacent navigation ─────────────────────────────────────────────────── */

void viewadjacent(const Arg *arg) {
  Namespace *ns = SELNS();
  int cur = __builtin_ctz(ns->tagset[ns->seltags]);
  int next = (cur + arg->i + NTAGS) % NTAGS;
  Arg a = {.ui = 1 << next};
  state_seltag(&a);
}

void tagadjacent(const Arg *arg) {
  Namespace *ns = SELNS();
  if (!ns->sel)
    return;
  int cur = __builtin_ctz(ns->sel->tags);
  int next = (cur + arg->i + NTAGS) % NTAGS;
  ns->sel->tags = 1 << next;
  /* follow the window to its new tag */
  Arg a = {.ui = 1 << next};
  state_seltag(&a);
  x11_focus(NULL);
  x11_arrange(wm.selmon);
  bar_draw(wm.selmon);
}

void viewadjacentns(const Arg *arg) {
  int next = (wm.selmon->ans + arg->i + NNAMESPACES) % NNAMESPACES;
  Arg a = {.i = next};
  switchns(&a);
}

void tagadjacentns(const Arg *arg) {
  Namespace *ns = SELNS();
  if (!ns->sel)
    return;
  Client *c = ns->sel;
  int next = (c->ns + arg->i + NNAMESPACES) % NNAMESPACES;
  if (next == c->ns)
    return;
  detach(c);
  detachstack(c);
  c->ns = next;
  c->tags = wm.ns[next].tagset[wm.ns[next].seltags];
  attach(c);
  attachstack(c);
  /* follow window to new namespace */
  Arg a = {.i = next};
  switchns(&a);
  x11_focus(NULL);
  x11_arrange(wm.selmon);
  bar_draw(wm.selmon);
}

void whichkey(const Arg *arg) {
  (void)arg;
  bar_whichkey_activate();
}

void x11_grabkeys(void) { grabkeys(); }
