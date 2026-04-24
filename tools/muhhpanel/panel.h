#ifndef PANEL_H
#define PANEL_H

#include "../colors.h"

/* ── Geometry ───────────────────────────────────── */
static const int panel_width_pct = 100;
static const int panel_left_gap = 0;
static const int panel_right_gap = 0;
static const int panel_height_pct = 94;

/* ── Appearance ─────────────────────────────────── */
static const char *panel_fonts[] = {"monospace:size=10"};
static const char *clock_fonts[] = {"monospace:size=18"};

static const char *panel_colors[][3] = {
    [0] = {COL_FG, COL_BG, COL_BORDER},           /* normal */
    [1] = {COL_BG, COL_ACCENT, COL_ACCENT},       /* accent / selected */
    [2] = {COL_BRIGHT_BLACK, COL_BG, COL_BORDER}, /* dim */
    [3] = {COL_RED, COL_BG, COL_BORDER},          /* warning */
};

/* ── Module lists ───────────────────────────────── */
static const char *left_modules[] = {"kde_notifications", "thoughts", "input",
                                     NULL};
static const char *middle_modules[] = {
    "daystrip", "calendar", "activity", "pomodoro", "music", "textsmenu", NULL};
static const char *bottom_modules[] = {NULL};

static const char *right_column_placeholder[] = {NULL};

static const char *right_clock_list[] = {"clock", NULL};
static const char *right_wifi_bt_list[] = {"wifi", "bluetooth", NULL};
static const char *right_vol_list[] = {"volume", NULL};
static const char *right_bri_list[] = {"brightness", NULL};
static const char *right_kde_cpu_scr_list[] = {"kde_connect", "cpu_governor",
                                               "screenshot", NULL};
static const char *right_power_list[] = {"power", NULL};
static const char *timeline_modules[] = {"timeline", NULL};
static const char *topstrip_modules[] = {"topstrip", NULL};

/* ── Layout rows ────────────────────────────────── */
static struct {
  int height_pct;
  int col_widths[4];
  const char **col_modules[4];
} layout_rows[] = {
    /* Row 0 – top strip (fixed 20 px) */
    {.height_pct = -20,
     .col_widths = {100, 0},
     .col_modules = {topstrip_modules, NULL}},
    /* Row 1 – left / middle / right (70% of remaining height) */
    {.height_pct = 70,
     .col_widths = {40, 35, 25, 0},
     .col_modules = {left_modules, middle_modules, right_column_placeholder,
                     NULL}},
    /* Row 2 – bottom row (30% of remaining height) */
    {.height_pct = 30,
     .col_widths = {100, 0},
     .col_modules = {bottom_modules, NULL}},
    /* Row 3 – timeline bar (fixed 13 px) */
    {.height_pct = -13,
     .col_widths = {100, 0},
     .col_modules = {timeline_modules, NULL}},
};
#define NUM_ROWS (sizeof(layout_rows) / sizeof(layout_rows[0]))

/* ── Container themes ──────────────────────────── */
extern ContainerTheme right_theme;
extern ContainerTheme module_card_theme;

#endif
