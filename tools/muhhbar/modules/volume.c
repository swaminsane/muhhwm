#define _POSIX_C_SOURCE 200809L
#include "common.h"

static int vol_pct = 0;
static int vol_muted = 0;

void volume_update(void) {
  FILE *f = popen("amixer get Master 2>/dev/null", "r");
  if (!f)
    return;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char *p = strstr(line, "[");
    if (!p)
      continue;
    if (sscanf(p, "[%d%%]", &vol_pct) == 1) {
      vol_muted = (strstr(line, "[off]") != NULL);
      break;
    }
  }
  pclose(f);
}

int volume_draw(int x) {
  char text[16];
  if (vol_muted)
    snprintf(text, sizeof text, "o %d", vol_pct);
  else
    snprintf(text, sizeof text, "v %d", vol_pct);
  drw_setscheme(drw, scheme[vol_muted ? SchGrey : SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)sysmods[MOD_VOLUME].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void volume_click(int button) {
  if (button == 1) {
    (void)system("amixer set Master toggle >/dev/null 2>&1");
    sysmods[MOD_VOLUME].updated = 0;
  } else if (button == 3) {
    static const char *cmd[] = {"pavucontrol", NULL};
    spawn(cmd);
  }
}

void volume_scroll(int dir) {
  if (dir > 0)
    (void)system("amixer set Master 1%+ >/dev/null 2>&1");
  else
    (void)system("amixer set Master 1%- >/dev/null 2>&1");
  sysmods[MOD_VOLUME].updated = 0;
}
