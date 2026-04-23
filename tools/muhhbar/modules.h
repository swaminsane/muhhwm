#ifndef MODULES_H
#define MODULES_H

#include <time.h>

#define MODE_SYSTEM 0
#define MODE_FOCUS 1
#define MODE_MEDIA 2
#define NMODES 3

#define MOD_BATTERY 0
#define MOD_BRIGHTNESS 1
#define MOD_VOLUME 2
#define MOD_NETWORK 3
#define MOD_STAT 4
#define MOD_TIME 5
#define MOD_POWER 6
#define NSYSMODS 7

#define MOD_ACTIVITY 0
#define MOD_STREAK 1
#define MOD_POMODORO 2
#define MOD_TEXTS 3
#define NFOCUSMODS 4

#define MOD_MEDIA 0
#define MOD_TV 1
#define NMEDIAMODS 2

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
extern Mod mediamods[NMEDIAMODS];

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
void power_update(void);
int power_draw(int x);
void power_click(int b);
void power_scroll(int d);
void power_draw_menu(void);
void power_menu_click(int ex);
extern int power_view;
extern time_t power_time;

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
void texts_update(void);
int texts_draw(int x);
void texts_click(int b);
void texts_scroll(int d);

void media_update(void);
int media_draw(int x);
void media_click(int b);
void media_scroll(int d);
void tv_update(void);
int tv_draw(int x);
void tv_click(int b);
void tv_scroll(int d);

void modules_init(void);
void focus_modules_init(void);
void media_modules_init(void);

#endif /* MODULES_H */
