#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "muhhtoggle: cannot open display\n");
    return 1;
  }
  Window root = DefaultRootWindow(dpy);
  Atom toggle = XInternAtom(dpy, "_MUHH_PANEL_TOGGLE", False);
  Atom wid_atom = XInternAtom(dpy, "_MUHH_PANEL_WID", False);

  Window panel_win = None;

  /* retry up to 10 times (1 second total) for panel to start */
  for (int retry = 0; retry < 10; retry++) {
    XSync(dpy, False);
    Atom type;
    int format;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, root, wid_atom, 0, 1, False, XA_WINDOW, &type,
                           &format, &n, &after, &data) == Success &&
        data) {
      panel_win = *(Window *)data;
      XFree(data);

      /* verify window actually exists */
      XWindowAttributes wa;
      if (XGetWindowAttributes(dpy, panel_win, &wa)) {
        break; /* window is valid */
      }
      panel_win = None;
    }
    usleep(100000); /* 100ms */
  }

  if (!panel_win) {
    fprintf(stderr, "muhhtoggle: panel not running or window not ready\n");
    return 1;
  }

  XEvent ev = {0};
  ev.type = ClientMessage;
  ev.xclient.window = panel_win;
  ev.xclient.message_type = toggle;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 1;
  XSendEvent(dpy, panel_win, False, NoEventMask, &ev);
  XFlush(dpy);

  XCloseDisplay(dpy);
  printf("Toggled panel.\n");
  return 0;
}
