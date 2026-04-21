#define _POSIX_C_SOURCE 200809L
#include "common.h"

void time_update(void) { /* drawn live, nothing to cache */ }

int time_draw(int x) {
  char text[8];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  strftime(text, sizeof text, "%H:%M", tm);
  drw_setscheme(drw, scheme[SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)sysmods[MOD_TIME].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void time_click(int button) {
  if (button == 1) {
    static const char *cmd[] = {"st",
                                "-e",
                                "sh",
                                "-c",
                                "echo; cal; echo; date '+%A, %B %d %Y'; echo; "
                                "date '+%H:%M:%S'; sleep 10",
                                NULL};
    spawn(cmd);
  }
}

void time_scroll(int dir) { (void)dir; }
