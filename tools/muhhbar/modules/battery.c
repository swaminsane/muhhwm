#define _POSIX_C_SOURCE 200809L
#include "common.h"

static int bat_pct = 0;
static int bat_charging = 0;
static int bat_sch = SchNorm;

void battery_update(void) {
  bat_pct = read_int_file("/sys/class/power_supply/BAT0/capacity");

  char buf[32] = {0};
  FILE *f = fopen("/sys/class/power_supply/BAT0/status", "r");
  if (f) {
    (void)fgets(buf, sizeof buf, f);
    fclose(f);
  }
  bat_charging = (strncmp(buf, "Charging", 8) == 0);

  if (bat_pct <= 10)
    bat_sch = SchCrit;
  else if (bat_pct <= 30)
    bat_sch = SchWarn;
  else
    bat_sch = SchNorm;
}

int battery_draw(int x) {
  int w = sysmods[MOD_BATTERY].width;
  int lp = lrpad / 2;

  char pct[8];
  snprintf(pct, sizeof pct, "%d%%", bat_pct);

  if (bat_charging) {
    // EVERYTHING GREEN when charging
    drw_setscheme(drw, scheme[SchGreen]);

    char buf[16];
    snprintf(buf, sizeof buf, "* %s", pct);

    drw_text(drw, x, 0, (unsigned int)w, (unsigned int)barh, (unsigned int)lp,
             buf, 0);
  } else {
    // normal color logic
    drw_setscheme(drw, scheme[bat_sch]);
    drw_text(drw, x, 0, (unsigned int)w, (unsigned int)barh, (unsigned int)lp,
             pct, 0);
  }

  return x + w;
}

void battery_click(int button) { (void)button; }
void battery_scroll(int dir) { (void)dir; }
