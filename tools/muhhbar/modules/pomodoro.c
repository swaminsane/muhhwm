#define _POSIX_C_SOURCE 200809L
#include "common.h"

static char pom_label[32] = "";
static long pom_remaining = 0;

void pomodoro_update(void) {
  FILE *f = fopen("/tmp/pomodoro_state", "r");
  if (!f) {
    pom_remaining = 0;
    pom_label[0] = '\0';
    return;
  }

  char line[64] = {0};
  (void)fgets(line, sizeof line, f);
  fclose(f);

  char *pipe = strchr(line, '|');
  if (!pipe) {
    pom_remaining = 0;
    return;
  }

  *pipe = '\0';
  memcpy(pom_label, line, sizeof pom_label - 1);
  pom_label[sizeof pom_label - 1] = '\0';
  long long end = atoll(pipe + 1);

  /* idle state written by script as "idle|0" */
  if (strcmp(pom_label, "idle") == 0 || end == 0) {
    pom_remaining = 0;
    return;
  }

  long long now = (long long)time(NULL);
  pom_remaining = (long)(end - now);
  if (pom_remaining < 0)
    pom_remaining = 0;
}

int pomodoro_draw(int x) {
  char text[24];

  if (pom_remaining > 0) {
    int mins = (int)(pom_remaining / 60);
    int secs = (int)(pom_remaining % 60);
    char label[12];
    memcpy(label, pom_label, 11);
    label[11] = '\0';
    snprintf(text, sizeof text, "%s %dm%02ds", label, mins, secs);
    drw_setscheme(drw, scheme[SchAccent]);
  } else {
    snprintf(text, sizeof text, "pom");
    drw_setscheme(drw, scheme[SchGrey]);
  }

  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_POMODORO].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void pomodoro_click(int button) {
  if (button == 1) {
    static const char *cmd[] = {"/bin/sh", "-c",
                                "$HOME/.local/bin/menu/pomodmenu", NULL};
    spawn(cmd);
  }
}

void pomodoro_scroll(int dir) { (void)dir; }
