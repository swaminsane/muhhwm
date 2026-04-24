#ifndef SETTINGS_H
#define SETTINGS_H

/* animation */
#define SHOW_STEPS 3          /* number of slide steps */
#define SHOW_STEP_DELAY_MS 10 /* ms between steps */

/* command runner history */
#define COMMAND_HISTORY_MAX 100 /* max persisted history lines, 0=off */
#define COMMAND_HISTORY_FILE "/tmp/muhhpanel_history"

/* module update intervals (seconds) */
#define CLOCK_UPDATE_IV 1
#define WIFI_POLL_IV 5
#define BT_POLL_IV 5
#define BATTERY_POLL_IV 60
#define CPU_GOV_POLL_IV 10
#define KDE_CONNECT_POLL_IV 10
#define MPRIS_POLL_IV 2
#define THOUGHTS_POLL_IV 5
#define TEXTSMENU_POLL_IV 5
#define DAYSTRIP_POLL_IV 60
#define CALENDAR_POLL_IV 60
#define ACTIVITY_POLL_IV 10
#define POMODORO_POLL_IV 1
#define TIMELINE_HEIGHT 13

/* paths / commands */
#define WIFI_CMD "nmcli"
#define BT_CMD "bluetoothctl"
#define VOLUME_CMD "pactl"
#define BRIGHTNESS_CMD "brightnessctl"
#define TEMPERATURE_CMD "xsct"
#define SCREENSHOT_CMD "scrot"
#define SCREENSHOT_DIR "/redmi/DCIM/Screenshots/linux"
#define TEXTSMENU_RECENT "/home/swaminsane/.cache/textsmenu_recent"
#define BAR_THOUGHTS_FILE "/home/swaminsane/sync/docs/bar_thoughts.md"
#define NOTES_MAXLEN 1024

#endif
