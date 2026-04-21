#define _POSIX_C_SOURCE 200809L
#include "common.h"

static int bri_pct = 0;
static char bri_dev[64] = "";

static void find_backlight(void) {
  FILE *f = popen("ls /sys/class/backlight/ 2>/dev/null | head -1", "r");
  if (!f)
    return;
  (void)fgets(bri_dev, sizeof bri_dev, f);
  pclose(f);
  char *nl = strchr(bri_dev, '\n');
  if (nl)
    *nl = '\0';
}

void brightness_update(void) {
  if (!bri_dev[0])
    find_backlight();
  if (!bri_dev[0])
    return;
  char cur_path[128], max_path[128];
  snprintf(cur_path, sizeof cur_path, "/sys/class/backlight/%s/brightness",
           bri_dev);
  snprintf(max_path, sizeof max_path, "/sys/class/backlight/%s/max_brightness",
           bri_dev);
  int cur = read_int_file(cur_path);
  int max = read_int_file(max_path);
  bri_pct = (max > 0) ? (int)((float)cur / max * 100.0f + 0.5f) : 0;
}

int brightness_draw(int x) {
  char text[16];
  snprintf(text, sizeof text, "λ %d", bri_pct);
  drw_setscheme(drw, scheme[SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)sysmods[MOD_BRIGHTNESS].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void brightness_click(int button) { (void)button; }

void brightness_scroll(int dir) {
  if (dir > 0)
    (void)system("brightnessctl set +1% >/dev/null 2>&1");
  else
    (void)system("brightnessctl set 1%- >/dev/null 2>&1");
  sysmods[MOD_BRIGHTNESS].updated = 0;
}
