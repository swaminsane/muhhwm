/* muhh.h - core types and state for muhhwm
 * single include for all translation units
 */

#ifndef MUHH_H
#define MUHH_H

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
#include <X11/keysym.h>

#include "drw.h"
#include "topbar.h"
#include "util.h"

/* ── constants ─────────────────────────────────────────────────────────── */

#define NNAMESPACES 3
#define NTAGS 6
#define NNAMELEN 32
#define MAXCOLORS 3 /* SchemeNorm, SchemeSel, SchemeUrg */
#define TEXTW(x) (drw_fontset_getwidth(wm.drw, (x)) + wm.lrpad)

/* ── forward declarations ───────────────────────────────────────────────── */

typedef struct Client Client;
typedef struct Monitor Monitor;
typedef struct Bar Bar;
typedef struct Namespace Namespace;

/* ── enums ──────────────────────────────────────────────────────────────── */

enum { CurNormal, CurResize, CurMove, CurLast };

enum { SchemeNorm, SchemeSel, SchemeUrg };

enum { WMProtocols, WMDelete, WMStateAtom, WMTakeFocus, WMLast };

enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast
};

enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  ClkLast
};
/* ── core types ─────────────────────────────────────────────────────────── */

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags; /* tag bitmask within namespace   */
  int isfloating;
  int ns; /* target namespace, -1 = active  */
} Rule;

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *);
  const Arg arg;
} Button;

/* ── tag state ──────────────────────────────────────────────────────────── */

typedef struct {
  float mfact;         /* master/stack ratio         */
  int nmaster;         /* clients in master area     */
  int sellt;           /* selected layout index      */
  const Layout *lt[2]; /* current + previous layout  */
  Client *focused;     /* last focused client        */
} Tag;

/* ── client ─────────────────────────────────────────────────────────────── */

struct Client {
  char name[256];
  char class[64];
  char instance[64];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch;
  int maxw, maxh, minw, minh;
  int hintsvalid;
  int bw, oldbw;
  unsigned int tags; /* tag bitmask within namespace */
  int ns;            /* owning namespace index       */
  int isfixed, isfloating, isurgent;
  int isfullscreen, neverfocus, oldstate;
  int iswallpaper; /* true → treat as desktop background */
  Window win;
  Client *next;
  Client *snext;
};

/* ── namespace ──────────────────────────────────────────────────────────── */

struct Namespace {
  char name[NNAMELEN];
  unsigned int tagset[2]; /* current + previous tagset  */
  int seltags;            /* which tagset is active     */
  Tag tags[NTAGS];
  Client *clients; /* all clients                */
  Client *stack;   /* focus stack                */
  Client *sel;     /* selected client            */
};

/* ── bar ────────────────────────────────────────────────────────────────── */

struct Bar {
  Window win;
  int x, y, w, h;
  int showbar;
  int topbar;
};

/* ── monitor ────────────────────────────────────────────────────────────── */

struct Monitor {
  int num;
  int mx, my, mw, mh; /* monitor geometry           */
  int wx, wy, ww, wh; /* window area geometry       */
  int ans;            /* active namespace index     */
  Bar bar;
  Monitor *next;
};

/* ── global state ───────────────────────────────────────────────────────── */

typedef struct {
  Namespace ns[NNAMESPACES];
  Monitor *mons;
  Monitor *selmon;
  Display *dpy;
  Window root;
  Window wmcheckwin;
  Drw *drw;
  Clr **scheme;
  Clr *nsscheme[NNAMESPACES];
  Cursor cursor[CurLast];
  Atom wmatom[WMLast];
  Atom netatom[NetLast];
  int screen;
  int sw, sh;
  int running;
  int ans; /* globally active namespace  */
  int lrpad;
} WMState;

extern WMState wm;

/* ── convenience macros ─────────────────────────────────────────────────── */

#define SELNS() (&wm.ns[wm.selmon->ans])
#define SELCLI() (SELNS()->sel)
#define CURTAG(ns) (&(ns)->tags[(__builtin_ctz((ns)->tagset[(ns)->seltags]))])
#define TAGMASK ((1 << NTAGS) - 1)
#define ISVISIBLE(c) ((c)->tags & SELNS()->tagset[SELNS()->seltags])
#define NOTES_MAXLEN 1024

/* ── function declarations ──────────────────────────────────────────────── */

/* state.c */
void state_init(void);
void state_switchns(Monitor *m, int ns);
void state_seltag(const Arg *arg);

/* serialize.c */
void serialize_save(void);
void serialize_restore(void);

/* x11.c */
void x11_init(void);
void x11_run(void);
void x11_manage(Window w, XWindowAttributes *wa);
void x11_unmanage(Client *c, int destroyed);
void x11_arrange(Monitor *m);
void x11_focus(Client *c);
void x11_scan(void);
void tile(Monitor *m);
void monocle(Monitor *m);
void focusstack(const Arg *arg);
void incnmaster(const Arg *arg);
void killclient(const Arg *arg);
void movemouse(const Arg *arg);
void quit(const Arg *arg);
void resizemouse(const Arg *arg);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void spawn(const Arg *arg);
void tag(const Arg *arg);
void togglebar(const Arg *arg);
void toggleboth(const Arg *arg);
void togglefloating(const Arg *arg);
void toggletag(const Arg *arg);
void zoom(const Arg *arg);
void switchns(const Arg *arg);
void viewadjacentns(const Arg *arg);
void tagadjacentns(const Arg *arg);
void viewadjacent(const Arg *arg);
void tagadjacent(const Arg *arg);
void whichkey(const Arg *arg);
void barnotes(const Arg *arg);
void x11_grabkeys(void);
void topbar_tick(void);

/* bar.c */
void bar_init(Monitor *m);
void bar_draw(Monitor *m);
void bar_click(Monitor *m, int x, int button);
void bar_whichkey_activate(void);
void bar_whichkey_key(KeySym ks);
int bar_whichkey_active(void);
void bar_whichkey_activate(void);
void bar_whichkey_key(KeySym ks);
int bar_whichkey_active(void);
void bar_notes_activate(void);
void bar_notes_key(KeySym ks, unsigned int state);
void bar_selection_notify(XEvent *e);
int bar_notes_active(void);
extern char stext[256];

/* fs.c */
void fs_init(void);
void fs_stop(void);

/* rules.c */
void rules_apply(Client *c);

/* activity.c */
void activity_init(void);
void activity_focus(Client *c);
void activity_update_title(Client *c);
void activity_flush(void);

/* strip.c */
#include "strip.h"
void strip_init(void);
void strip_tick(void);
void strip_draw(void);
void strip_set_ns(int ns);
void strip_motion(int x, int y);
void strip_click(int y, int button);
int strip_window(void);
void togglestrip(const Arg *arg);
Window strip_win(void);

#endif /* MUHH_H */
