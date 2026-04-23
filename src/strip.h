
/* strip.h - muhhwm day strip declarations */

#ifndef STRIP_H
#define STRIP_H

#include <X11/Xlib.h>

/* ── public API ──────────────────────────────────────────────────────────── */

void strip_init(void);     /* call after x11_init, creates window          */
void strip_tick(void);     /* call every minute from main loop              */
void strip_draw(void);     /* redraw the strip                              */
void strip_set_ns(int ns); /* called when namespace changes (0/1/2)        */
void strip_motion(int x, int y); /* called from motionnotify for hover        */
void strip_click(int y, int button); /* called from buttonpress on strip win  */
int strip_window(void); /* returns 1 if w is the strip window            */
Window strip_win(void); /* returns the strip X window                   */

#endif /* STRIP_H */
