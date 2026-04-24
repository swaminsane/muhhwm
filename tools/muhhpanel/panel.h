#ifndef PANEL_H
#define PANEL_H

/* ── Geometry ───────────────────────────────────── */
static const int panel_width_pct = 100;
static const int panel_left_gap = 25;
static const int panel_right_gap = 25;
static const int panel_height_pct = 90;

/* ── Appearance ─────────────────────────────────── */
static const char *panel_fonts[] = {"monospace:size=10"};
static const char *clock_fonts[] = {"monospace:size=18"};

static const char *panel_colors[][3] = {
    [0] = {"#d8dee9", "#2e3440", "#3b4252"},
    [1] = {"#2e3440", "#88c0d0", "#88c0d0"},
    [2] = {"#4c566a", "#2e3440", "#4c566a"},
    [3] = {"#bf616a", "#2e3440", "#bf616a"},
};

/* ── Modules for each column ────────────────────── */
static const char *left_modules[] = {"kde_notifications", "thoughts", "input",
                                     NULL};
static const char *middle_modules[] = {
    "daystrip", "calendar", "activity", "pomodoro", "music", "textsmenu", NULL};
static const char *right_modules[] = {
    "clock",       "wifi",         "bluetooth",  "volume", "brightness",
    "kde_connect", "cpu_governor", "screenshot", "power",  NULL};
static const char *bottom_modules[] = {"command_runner", NULL};
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
     .col_widths = {33, 34, 33, 0},
     .col_modules = {left_modules, middle_modules, right_modules, NULL}},
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

#endif
