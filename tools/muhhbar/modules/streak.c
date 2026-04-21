#define _POSIX_C_SOURCE 200809L
#include "common.h"

static char streak_ns[32] = "";
static time_t streak_start = 0;

void streak_update(void) {
  char cur[32] = "";
  FILE *f = fopen("/tmp/muhh/active_namespace", "r");
  if (f) {
    (void)fgets(cur, sizeof cur, f);
    fclose(f);
    char *nl = strchr(cur, '\n');
    if (nl)
      *nl = '\0';
  }

  if (strcmp(cur, streak_ns) != 0) {
    memcpy(streak_ns, cur, sizeof streak_ns - 1);
    streak_ns[sizeof streak_ns - 1] = '\0';
    streak_start = time(NULL);
  }
}

int streak_draw(int x) {
  char text[16];
  long elapsed = streak_start ? (long)(time(NULL) - streak_start) : 0;
  int mins = (int)(elapsed / 60);
  if (mins > 999)
    mins = 999;
  snprintf(text, sizeof text, "-> %dm", mins);
  drw_setscheme(drw, scheme[SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_STREAK].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void streak_click(int button) { (void)button; }
void streak_scroll(int dir) { (void)dir; }
