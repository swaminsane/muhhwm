#define _POSIX_C_SOURCE 200809L
#include "common.h"

/* power menu state — stored in muhhbar globals */
/* reuse detail_view pattern but with a separate flag */
int power_view = 0;
time_t power_time = 0;
#define POWER_TIMEOUT 10

#define NPOWOPTS 5
static const char *pow_labels[NPOWOPTS] = {"[Lock]", "[Log Out]", "[Sleep]",
                                           "[Reboot]", "[Shut Down]"};
static const char *pow_cmds[NPOWOPTS] = {
    "slock", "pkill -9 muhhwm", "systemctl suspend", "systemctl reboot",
    "systemctl poweroff"};

/* x positions of each option set at draw time */
static int pow_x[NPOWOPTS];

void power_update(void) { /* nothing to poll */ }

int power_draw(int x) {
  drw_setscheme(drw, scheme[SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)sysmods[MOD_POWER].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), "[φ]", 0);
}

/* called from muhhbar.c draw() when power_view is active */
void power_draw_menu(void) {
  int i, x = 0;
  for (i = 0; i < NPOWOPTS; i++) {
    int w = TEXTW(pow_labels[i]);
    pow_x[i] = x;
    drw_setscheme(drw, scheme[SchNorm]);
    drw_text(drw, x, 0, (unsigned int)w, (unsigned int)barh,
             (unsigned int)(lrpad / 2), pow_labels[i], 0);
    x += w;
  }
}

/* called from muhhbar.c handle_button() when power_view is active */
void power_menu_click(int ex) {
  int i;
  for (i = 0; i < NPOWOPTS; i++) {
    int w = TEXTW(pow_labels[i]);
    if (ex >= pow_x[i] && ex < pow_x[i] + w) {
      const char *cmd[] = {"/bin/sh", "-c", pow_cmds[i], NULL};
      spawn(cmd);
      power_view = 0;
      return;
    }
  }
  /* clicked outside any option — collapse */
  power_view = 0;
}

void power_click(int button) {
  if (button == 1) {
    power_view = 1;
    power_time = time(NULL);
  }
}

void power_scroll(int dir) { (void)dir; }
