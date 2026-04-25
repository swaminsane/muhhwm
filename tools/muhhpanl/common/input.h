#ifndef INPUT_H
#define INPUT_H

#include "module.h" /* for InputEvent, EventType */
#include <X11/Xlib.h>

/* Translate a raw X11 event into the unified InputEvent format.
 * Returns 1 if the event was translated, 0 if it should be ignored. */
int translate_event(XEvent *xev, InputEvent *iev);

#endif
