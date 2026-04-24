#ifndef INPUT_H
#define INPUT_H

enum EventType {
  EV_PRESS,     /* mouse button pressed */
  EV_RELEASE,   /* mouse button released */
  EV_MOTION,    /* mouse moved */
  EV_SCROLL,    /* scroll wheel / two‑finger */
  EV_KEY_PRESS, /* keyboard key pressed */
};

typedef struct InputEvent {
  int type;             /* one of EventType */
  int x, y;             /* relative to module’s top‑left */
  int root_x, root_y;   /* absolute screen position (for hit‑testing) */
  unsigned int button;  /* Button1‑5, 0 if none */
  int scroll_dir;       /* 1 = up, -1 = down */
  unsigned int keycode; /* X11 keycode */
  unsigned int state;   /* modifier mask */
} InputEvent;

#endif
