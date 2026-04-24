#ifndef PANEL_GLOBALS_H
#define PANEL_GLOBALS_H

#include "drw.h"
#include <X11/Xlib.h>

extern Display *dpy;
extern Window root;
extern Drw *drw;
extern Clr **scheme;
extern int panel_w, panel_h;

void panel_redraw(void);
void panel_hide(void);

#endif
