/* config.h - muhhwm user configuration */

#ifndef CONFIG_H
#define CONFIG_H

#include "/home/swaminsane/.config/theme/colors.h"
#include "src/muhh.h"

/* ── appearance ─────────────────────────────────────────────────────────── */

static const char *fonts[] = {"monospace:size=10"};
static const char dmenufont[] = "monospace:size=10";
static const unsigned int borderpx = 1;
static const unsigned int snap = 32;
static const int showbar = 1;
static const int topbar = 1;

static const char *colors[][3] = {
    /*                    fg          bg          border      */
    [SchemeNorm] = {COL_FG, COL_BG, COL_BORDER},
    [SchemeSel] = {COL_BG, COL_ACCENT, COL_ACCENT},
    [SchemeUrg] = {COL_BG, COL_RED, COL_RED},
};

/* ── namespaces ─────────────────────────────────────────────────────────── */

static const char *nsnames[NNAMESPACES] = {
    "study",
    "code",
    "free",
};

/* ── namespaces tags ─────────────────────────────────────────────────────── */

static const char *tags[NTAGS] = {"1", "2", "3", "4", "5", "6"};

/* namespace accent colors shown in bar [ns index][fg, bg] */
static const char *nscolors[NNAMESPACES][2] = {
    {COL_BG, COL_BLUE},    /* study */
    {COL_BG, COL_GREEN},   /* code  */
    {COL_BG, COL_MAGENTA}, /* free  */
};

/* ── layouts ────────────────────────────────────────────────────────────── */

static const float mfact = 0.55;
static const int nmaster = 1;
static const int resizehints = 0;
static const unsigned int refreshrate = 60;
/* layout functions forward declared in x11.c */

static const Layout layouts[] = {
    {"[]=", tile}, {"[M]", monocle}, {"><>", NULL}, /* floating */
};

/* ── window rules ───────────────────────────────────────────────────────── */

static const Rule rules[] = {
    /* class        instance  title   tags  float  ns  */
    {"Zathura", NULL, NULL, 1 << 1, 0, 0}, /* study/tag2 */
    {"firefox", NULL, NULL, 1 << 0, 0, 0}, /* study/tag1 */
    {"mpv", NULL, NULL, 1 << 1, 0, 2},     /* free/tag2  */
    {"st", NULL, NULL, 1 << 0, 0, 1},      /* code/tag1  */
};

/* ── commands ───────────────────────────────────────────────────────────── */

static const char *termcmd[] = {"st", NULL};
static const char *dmenucmd[] = {"dmenu_run", NULL};

/* ── keybindings ────────────────────────────────────────────────────────── */

#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                      \
  {MODKEY, KEY, state_seltag, {.ui = 1 << TAG}},                               \
      {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},

static const Key keys[] = {
    /* modifier              key        function          argument */
    {MODKEY, XK_Return, spawn, {.v = termcmd}},
    {MODKEY, XK_p, spawn, {.v = dmenucmd}},
    {MODKEY, XK_b, togglebar, {0}},
    {MODKEY, XK_j, focusstack, {.i = +1}},
    {MODKEY, XK_k, focusstack, {.i = -1}},
    {MODKEY, XK_i, incnmaster, {.i = +1}},
    {MODKEY, XK_d, incnmaster, {.i = -1}},
    {MODKEY, XK_h, setmfact, {.f = -0.05}},
    {MODKEY, XK_l, setmfact, {.f = +0.05}},
    {MODKEY, XK_space, setlayout, {0}},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}},
    {MODKEY | ShiftMask, XK_c, killclient, {0}},
    {MODKEY | ShiftMask, XK_q, quit, {0}},
    /* namespace switching */
    {MODKEY, XK_F1, switchns, {.i = 0}},
    {MODKEY, XK_F2, switchns, {.i = 1}},
    {MODKEY, XK_F3, switchns, {.i = 2}},
    /* tags 1-6 */
    TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
        TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5)};

/* ── mouse buttons ──────────────────────────────────────────────────────── */

static const Button buttons[] = {
    {ClkTagBar, 0, Button1, state_seltag, {0}},
    {ClkTagBar, 0, Button3, toggletag, {0}},
    {ClkWinTitle, 0, Button2, zoom, {0}},
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
};

#endif /* CONFIG_H */
