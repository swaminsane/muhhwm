#ifndef MODULES_H
#define MODULES_H

#include <time.h>

/* modes */
#define MODE_SYSTEM 0
#define MODE_FOCUS 1
#define MODE_MEDIA 2
#define NMODES 3

/* system mode module indices */
#define MOD_BATTERY 0
#define MOD_BRIGHTNESS 1
#define MOD_VOLUME 2
#define MOD_NETWORK 3
#define MOD_STAT 4
#define MOD_TIME 5
#define NSYSMODS 6

/* focus mode module indices */
#define MOD_ACTIVITY 0
#define MOD_STREAK 1
#define MOD_POMODORO 2
#define NFOCUSMODS 3

typedef struct {
  int x;
  int width;
  int interval;
  time_t updated;
  void (*update)(void);
  int (*draw)(int x);
  void (*click)(int button);
  void (*scroll)(int dir);
} Mod;

extern Mod sysmods[NSYSMODS];
extern Mod focusmods[NFOCUSMODS];

/* system module functions */
void battery_update(void);
int battery_draw(int x);
void battery_click(int b);
void battery_scroll(int d);
void brightness_update(void);
int brightness_draw(int x);
void brightness_click(int b);
void brightness_scroll(int d);
void volume_update(void);
int volume_draw(int x);
void volume_click(int b);
void volume_scroll(int d);
void network_update(void);
int network_draw(int x);
void network_click(int b);
void network_scroll(int d);
void stat_update(void);
int stat_draw(int x);
void stat_click(int b);
void stat_scroll(int d);
void time_update(void);
int time_draw(int x);
void time_click(int b);
void time_scroll(int d);

/* focus module functions */
void activity_update(void);
int activity_draw(int x);
void activity_click(int b);
void activity_scroll(int d);
void streak_update(void);
int streak_draw(int x);
void streak_click(int b);
void streak_scroll(int d);
void pomodoro_update(void);
int pomodoro_draw(int x);
void pomodoro_click(int b);
void pomodoro_scroll(int d);

void modules_init(void);
void focus_modules_init(void);

#endif /* MODULES_H */
