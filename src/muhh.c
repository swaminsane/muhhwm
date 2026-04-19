
/* muhh.c - muhhwm entry point */

#define _POSIX_C_SOURCE 200809L

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "muhh.h"

/* global state - single instance, accessed everywhere via wm.something */
WMState wm;

static void sigchld_handler(int signo) {
  (void)signo;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

static void setup_signals(void) {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);
}

static void usage(void) {
  fputs("usage: muhhwm [-v]\n", stderr);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc == 2 && !strcmp(argv[1], "-v")) {
    puts("muhhwm-" VERSION);
    return 0;
  }
  if (argc != 1)
    usage();

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("muhhwm: warning: no locale support\n", stderr);

  setup_signals();

  /* init order:
   * 1. state  - pure data, no dependencies
   * 2. display - open X connection
   * 3. x11   - X setup, atoms, fonts, monitors, bar
   * 4. fs    - FUSE mount, runs in background thread
   * 5. scan  - adopt existing windows
   * 6. restore - apply saved session state over scanned windows
   */
  state_init();

  wm.dpy = XOpenDisplay(NULL);
  if (!wm.dpy)
    die("muhhwm: cannot open display");

  x11_init();
  fs_init();
  x11_scan();
  serialize_restore();

  /* main event loop - blocks until wm.running = 0 */
  x11_run();

  /* clean exit - save state before tearing down */
  serialize_save();
  fs_stop();

  return 0;
}
