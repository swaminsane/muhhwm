#ifndef MUHHBAR_H
#define MUHHBAR_H

#include "../../colors.h"
#include "drw.h"

/* font — resolved at build time via -I flag to reach fonts.h */
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

#define TEXTW(t) ((int)drw_fontset_getwidth(drw, (t)) + lrpad)

void spawn(const char **cmd);

#endif /* MUHHBAR_H */
