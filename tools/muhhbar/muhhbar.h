#ifndef MUHHBAR_H
#define MUHHBAR_H

#include "drw.h"

#ifndef FONT_MAIN
#define FONT_MAIN "monospace:size=10"
#endif

/* color scheme indices */
enum {
  SchNorm,
  SchAccent,
  SchBlock,
  SchWarn,
  SchCrit,
  SchGreen,
  SchGrey,
  SchNsStudy, /* blue  — study namespace */
  SchNsCode,  /* green — code namespace  */
  SchNsFree,  /* magenta — free namespace */
  SchLast
};

/* shared globals — defined in muhhbar.c */
extern Display *dpy;
extern Window root;
extern int screen;
extern Drw *drw;
extern Clr **scheme;
extern int lrpad;
extern int barh;
extern int barw;

/* detail view state — set by activity module, read by draw() */
extern int detail_view;
extern time_t detail_time;
#define DETAIL_TIMEOUT 10

#define TEXTW(t) ((int)drw_fontset_getwidth(drw, (t)) + lrpad)

void spawn(const char **cmd);

#endif /* MUHHBAR_H */
