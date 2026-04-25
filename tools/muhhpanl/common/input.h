#ifndef INPUT_H
#define INPUT_H

#include <X11/Xlib.h>

typedef enum {
  EV_PRESS,
  EV_RELEASE,
  EV_SCROLL,
  EV_MOTION,
  EV_ENTER,
  EV_LEAVE,
  EV_KEY_PRESS,
  EV_KEY_RELEASE,
} EventType;

typedef struct InputEvent {
  EventType type;

  int x, y;
  int root_x, root_y;

  unsigned int button;

  int scroll_dx, scroll_dy;

  unsigned int keycode;
  unsigned int state;
} InputEvent;

int translate_event(XEvent *xev, InputEvent *iev);

#endif
