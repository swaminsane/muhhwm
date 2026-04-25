#ifndef PANEL_H
#define PANEL_H

#include "../../colors.h"
#include "common/container.h" /* LayoutNode, container_build_tree, right_theme, … */

/* ── Geometry ───────────────────────────────────── */
static const int panel_width_pct = 100;
static const int panel_left_gap = 0;
static const int panel_right_gap = 0;
static const int panel_height_pct = 80;

/* ── Appearance ─────────────────────────────────── */
static const char *panel_fonts[] = {"monospace:size=10"};
static const char *clock_fonts[] = {"monospace:size=18"};

static const char *panel_colors[][3] = {
    [0] = {COL_FG, COL_BG, COL_BORDER},
    [1] = {COL_BG, COL_ACCENT, COL_ACCENT},
    [2] = {COL_BRIGHT_BLACK, COL_BG, COL_BORDER},
    [3] = {COL_RED, COL_BG, COL_BORDER},
};

/* ── Module name strings (used directly by tree leaves) ───── */
#define M_TOPSTRIP "topstrip"
#define M_LEFT_KDE "kde_notifications"
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
#define M_RIGHT_KDE "kde_connect"
#define M_RIGHT_CPU "cpu_governor"
#define M_RIGHT_SCR "screenshot"
#define M_RIGHT_PWR "power"
#define M_TIMELINE "timeline"

/* ═══════════════════════════════════════════════════════════════
 *  DECLARATIVE LAYOUT TREE
 * ═══════════════════════════════════════════════════════════════ */
static LayoutNode layout_tree = {
    .type = LAYOUT_COL,  /* vertical stack of rows */
    .children = (LayoutNode[]) {

        /* ---------- Row 0 : topstrip (fixed 20 px) ---------- */
        { .type = LAYOUT_ROW,
          .fixed_px = 20,
          .theme = &right_theme,
          .children = (LayoutNode[]) {
              { .type = LAYOUT_MODULE, .module_name = M_TOPSTRIP, .weight = 1.0f },
              { .type = 0 }
          },
          .nchildren = 1
        },

        /* ---------- Row 1 : main area (weight 70) ---------- */
        { .type = LAYOUT_ROW,
          .weight = 70.0f,
          .children = (LayoutNode[]) {

              /* left column (weight 33) */
              { .type = LAYOUT_COL,
                .weight = 33.0f,
                .children = (LayoutNode[]) {
                    { .type = LAYOUT_MODULE, .module_name = M_LEFT_KDE,       .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_LEFT_THOUGHTS,  .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_LEFT_INPUT,     .weight = 1.0f },
                    { .type = 0 }
                },
                .nchildren = 3
              },

              /* middle column (weight 34) */
              { .type = LAYOUT_COL,
                .weight = 34.0f,
                .children = (LayoutNode[]) {
                    { .type = LAYOUT_MODULE, .module_name = M_MID_DAYSTRIP,   .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_MID_CALENDAR,   .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_MID_ACTIVITY,   .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_MID_POMODORO,   .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_MID_MUSIC,      .weight = 1.0f },
                    { .type = LAYOUT_MODULE, .module_name = M_MID_TEXTSMENU,  .weight = 1.0f },
                    { .type = 0 }
                },
                .nchildren = 6
              },

              /* right column (weight 33) – nested structure */
              { .type = LAYOUT_COL,
                .weight = 33.0f,
                .theme = &right_theme,
                .children = (LayoutNode[]) {

                    /* clock */
                    { .type = LAYOUT_MODULE, .module_name = M_RIGHT_CLOCK, .weight = 1.0f },

                    /* wifi + bt grid (2 columns) */
                    { .type = LAYOUT_ROW,
                      .weight = 1.0f,
                      .children = (LayoutNode[]) {
                          { .type = LAYOUT_MODULE, .module_name = M_RIGHT_WIFI, .weight = 1.0f },
                          { .type = LAYOUT_MODULE, .module_name = M_RIGHT_BT,   .weight = 1.0f },
                          { .type = 0 }
                      },
                      .nchildren = 2
                    },

                    /* volume */
                    { .type = LAYOUT_MODULE, .module_name = M_RIGHT_VOL, .weight = 1.0f },

                    /* brightness */
                    { .type = LAYOUT_MODULE, .module_name = M_RIGHT_BRI, .weight = 1.0f },

                    /* kde + cpu + screenshot grid (3 columns) */
                    { .type = LAYOUT_ROW,
                      .weight = 1.0f,
                      .children = (LayoutNode[]) {
                          { .type = LAYOUT_MODULE, .module_name = M_RIGHT_KDE, .weight = 1.0f },
                          { .type = LAYOUT_MODULE, .module_name = M_RIGHT_CPU, .weight = 1.0f },
                          { .type = LAYOUT_MODULE, .module_name = M_RIGHT_SCR, .weight = 1.0f },
                          { .type = 0 }
                      },
                      .nchildren = 3
                    },

                    /* power */
                    { .type = LAYOUT_MODULE, .module_name = M_RIGHT_PWR, .weight = 1.0f },

                    { .type = 0 }
                },
                .nchildren = 6   /* clock, grid2, vol, bri, grid3, power */
              },

              { .type = 0 }
          },
          .nchildren = 3
        },

        /* ---------- Row 2 : bottom (weight 30) ---------- */
        { .type = LAYOUT_ROW,
          .weight = 30.0f,
          .children = (LayoutNode[]) {
              { .type = LAYOUT_MODULE, .module_name = NULL, .weight = 1.0f },
              { .type = 0 }
          },
          .nchildren = 1
        },

        /* ---------- Row 3 : timeline (fixed 13 px) ---------- */
        { .type = LAYOUT_ROW,
          .fixed_px = 13,
          .children = (LayoutNode[]) {
              { .type = LAYOUT_MODULE, .module_name = M_TIMELINE, .weight = 1.0f },
              { .type = 0 }
          },
          .nchildren = 1
        },

        { .type = 0 }
    },
    .nchildren = 4
};

#endif
