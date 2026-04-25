#include "input.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <string.h> /* for memset */

int translate_event(XEvent *xev, InputEvent *iev) {
  memset(iev, 0, sizeof(*iev));

  switch (xev->type) {
  case ButtonPress:
    iev->type = EV_PRESS;
    iev->button = xev->xbutton.button;
    iev->x = xev->xbutton.x;
    iev->y = xev->xbutton.y;
    iev->root_x = xev->xbutton.x_root;
    iev->root_y = xev->xbutton.y_root;
    iev->state = xev->xbutton.state;

    /* scroll buttons: translate to EV_SCROLL with delta */
    switch (xev->xbutton.button) {
    case Button4:
      iev->type = EV_SCROLL;
      iev->scroll_dy = 1;
      return 1; /* up */
    case Button5:
      iev->type = EV_SCROLL;
      iev->scroll_dy = -1;
      return 1; /* down */
    case 6:
      iev->type = EV_SCROLL;
      iev->scroll_dx = 1;
      return 1; /* left */
    case 7:
      iev->type = EV_SCROLL;
      iev->scroll_dx = -1;
      return 1; /* right */
    }
    return 1;

  case ButtonRelease:
    iev->type = EV_RELEASE;
    iev->button = xev->xbutton.button;
    iev->x = xev->xbutton.x;
    iev->y = xev->xbutton.y;
    iev->root_x = xev->xbutton.x_root;
    iev->root_y = xev->xbutton.y_root;
    iev->state = xev->xbutton.state;
    return 1;

  case MotionNotify:
    iev->type = EV_MOTION;
    iev->x = xev->xmotion.x;
    iev->y = xev->xmotion.y;
    iev->root_x = xev->xmotion.x_root;
    iev->root_y = xev->xmotion.y_root;
    iev->state = xev->xmotion.state;
    return 1;

  case KeyPress:
    iev->type = EV_KEY_PRESS;
    iev->keycode = xev->xkey.keycode;
    iev->state = xev->xkey.state;
    iev->root_x = xev->xkey.x_root;
    iev->root_y = xev->xkey.y_root;
    return 1;

  default:
    return 0;
  }
}
