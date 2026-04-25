#ifndef PANEL_GLOBALS_H
#define PANEL_GLOBALS_H

#include "drw.h"
#include <X11/Xlib.h>

/* shared globals – defined in muhhpanl.c */
extern Display *dpy;
extern Window root;
extern Drw *drw;
extern Clr **scheme;
extern int panel_w, panel_h;
extern Window panel_win;
extern int screen;
extern int panel_shown;
extern int panel_x;
extern int panel_y;

void panel_redraw(void); /* request a full redraw */
void panel_hide(void);   /* close the panel */

#endif
