#ifndef TOPBAR_H
#define TOPBAR_H
#include <X11/Xlib.h>

extern Window top_win;

void topbar_init(void);
int topbar_handle_event(XEvent *e);
void topbar_update_visibility(void);
void topbar_tick(void);

#endif
