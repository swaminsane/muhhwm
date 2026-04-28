#ifndef PANEL_H
#define PANEL_H

#include "../../colors.h"
#include "common/container.h"

/* ── Geometry ───────────────────────────────────── */
static const int panel_width_pct = 100;
static const int panel_left_gap = 0;
static const int panel_right_gap = 0;
static const int panel_height_pct = 87;

/* ── Module name strings ────────────────────────── */
#define M_TOPSTRIP "topstrip"
#define M_LEFT_THOUGHTS "thoughts"
#define M_LEFT_INPUT "input"
#define M_MID_DAYSTRIP "daystrip"
#define M_MID_CALENDAR "calendar"
#define M_MID_ACTIVITY "activity"
#define M_MID_POMODORO "pomodoro"
#define M_MID_MUSIC "music"
#define M_MID_TEXTSMENU "textsmenu"
#define M_RIGHT_CLOCK "clock"
#define M_RIGHT_WIFI "wifi"
#define M_RIGHT_BT "bluetooth"
#define M_RIGHT_VOL "volume"
#define M_RIGHT_BRI "brightness"
#define M_RIGHT_CPU "cpu_governor"
#define M_RIGHT_SCR "screenshot"
#define M_RIGHT_PWR "power"
#define M_AUDIOSINK "audiosink"
#define M_TIMELINE "timeline"
#define M_PROFANITY "profanity"
#define M_MPVBOX "mpvbox"
#define M_MPVSEARCH "mpvsearch"
#define M_SMS_INPUT "smsinput"
#define M_ADB "adb"
#define M_WEATHER "weather"

/* ═══════════════════════════════════════════════════
 *  DECLARATIVE LAYOUT TREE – defined in muhhpanl.c
 * ═══════════════════════════════════════════════════ */
extern LayoutNode layout_tree;

#endif
