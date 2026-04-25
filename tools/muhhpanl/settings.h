#ifndef SETTINGS_H
#define SETTINGS_H

/* ─── animation ───────────────────────────────── */
#define SHOW_STEPS 3
#define SHOW_STEP_DELAY_MS 10

/* ─── spacing ─────────────────────────────────── */
#define MODULE_VGAP 6
#define MODULE_HGAP 12
#define CONTAINER_PADDING 4

/* ─── update intervals (seconds) ──────────────── */
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

/* ─── module parameters ───────────────────────── */
#define VOLUME_STEP 5
#define BRIGHTNESS_STEP 5
#define TEMP_STEP 100
#define GOVERNOR_CYCLE "performance ondemand powersave"
#define SCREENSHOT_DIR "/redmi/DCIM/Screenshots/linux"

/* ─── power commands ──────────────────────────── */
#define LOCK_CMD "slock"
#define SLEEP_CMD "systemctl suspend"
#define REBOOT_CMD "systemctl reboot"
#define SHUTDOWN_CMD "systemctl poweroff"
#define LOGOUT_CMD "pkill -9 muhhwm"

/* ─── behaviour flags ─────────────────────────── */
#define WIFI_SHOW_SIGNAL_BARS 1
#define BT_SHOW_DEVICE_TYPE 1
#define VOLUME_MUTE_ON_CLICK 1
#define SCREENSHOT_CONFIRM_NOTIFY 1

#endif
