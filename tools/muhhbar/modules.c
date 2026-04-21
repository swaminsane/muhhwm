#define _POSIX_C_SOURCE 200809L
#include "modules.h"
#include "config.h"
#include "muhhbar.h"
#include <time.h>

Mod sysmods[NSYSMODS] = {
    [MOD_BATTERY] = {.interval = 15,
                     .update = battery_update,
                     .draw = battery_draw,
                     .click = battery_click,
                     .scroll = battery_scroll},
    [MOD_BRIGHTNESS] = {.interval = 1,
                        .update = brightness_update,
                        .draw = brightness_draw,
                        .click = brightness_click,
                        .scroll = brightness_scroll},
    [MOD_VOLUME] = {.interval = 1,
                    .update = volume_update,
                    .draw = volume_draw,
                    .click = volume_click,
                    .scroll = volume_scroll},
    [MOD_NETWORK] = {.interval = 10,
                     .update = network_update,
                     .draw = network_draw,
                     .click = network_click,
                     .scroll = network_scroll},
    [MOD_STAT] = {.interval = 10,
                  .update = stat_update,
                  .draw = stat_draw,
                  .click = stat_click,
                  .scroll = stat_scroll},
    [MOD_TIME] = {.interval = 1,
                  .update = time_update,
                  .draw = time_draw,
                  .click = time_click,
                  .scroll = time_scroll},
};

Mod focusmods[NFOCUSMODS] = {
    [MOD_ACTIVITY] = {.interval = 300,
                      .update = activity_update,
                      .draw = activity_draw,
                      .click = activity_click,
                      .scroll = activity_scroll},
    [MOD_STREAK] = {.interval = 1,
                    .update = streak_update,
                    .draw = streak_draw,
                    .click = streak_click,
                    .scroll = streak_scroll},
    [MOD_POMODORO] = {.interval = 1,
                      .update = pomodoro_update,
                      .draw = pomodoro_draw,
                      .click = pomodoro_click,
                      .scroll = pomodoro_scroll},
};

Mod mediamods[NMEDIAMODS] = {
    [MOD_MEDIA] = {.interval = 2,
                   .update = media_update,
                   .draw = media_draw,
                   .click = media_click,
                   .scroll = media_scroll},
};

void modules_init(void) {
  int i;
  sysmods[MOD_BATTERY].width = TEXTW("100%");
  sysmods[MOD_BRIGHTNESS].width = TEXTW("λ 100");
  sysmods[MOD_VOLUME].width = TEXTW("v 100");
  sysmods[MOD_NETWORK].width = TEXTW("W B");
  sysmods[MOD_STAT].width = TEXTW("θ 100°");
  sysmods[MOD_TIME].width = TEXTW("23:59:59");
  for (i = 0; i < NSYSMODS; i++) {
    sysmods[i].update();
    sysmods[i].updated = time(NULL);
  }
}

void focus_modules_init(void) {
  int i;
  focusmods[MOD_ACTIVITY].width = TEXTW("S 99h59m");
  focusmods[MOD_STREAK].width = TEXTW("-> 999m");
  focusmods[MOD_POMODORO].width = TEXTW("Pomodoro 99m59s");
  for (i = 0; i < NFOCUSMODS; i++) {
    focusmods[i].update();
    focusmods[i].updated = time(NULL);
  }
}

void media_modules_init(void) {
  int i;
  mediamods[MOD_MEDIA].width = TEXTW("> YT xxxxxxxxxxxxxxxxxxxxxxxx");
  for (i = 0; i < NMEDIAMODS; i++) {
    mediamods[i].update();
    mediamods[i].updated = time(NULL);
  }
}
