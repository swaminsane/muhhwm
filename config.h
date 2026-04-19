/* config.h - muhhwm user configuration */

#ifndef CONFIG_H
#define CONFIG_H

#include "/home/swaminsane/.config/theme/colors.h"
#include "/home/swaminsane/.config/theme/fonts.h"
#include "src/muhh.h"
#include <X11/XF86keysym.h>
/* ── appearance ─────────────────────────────────────────────────────────── */

static const char *fonts[] = {
    FONT_MAIN, "Noto Color Emoji:pixelsize=11:antialias=true:autohint=true"};
static const char dmenufont[] = FONT_MAIN;
static const unsigned int borderpx = BORDER_PX;
static const unsigned int snap = 22;
static const int showbar = 1;
static const int topbar = 0;

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

static const char *tags[NTAGS] = {"α", "β", "γ", "δ", "ε", "ζ"};

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
    {"firefox", NULL, NULL, 1 << 0, 0, 0},
    {"mpv", NULL, NULL, 1 << 1, 0, 2}, /* free/tag2  */
    {"st", NULL, NULL, 1 << 0, 0, 1},  /* code/tag1  */
};

/* ── commands ───────────────────────────────────────────────────────────── */

// static const char *termcmd[] = {"st", NULL};
static const char *dmenucmd[] = {"dmenu_run", NULL};
static const char *tabtermcmd[] = {"tabbed", "-r 2", "st", "-w", "''", NULL};

/* ── keybindings ────────────────────────────────────────────────────────── */

#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                      \
  {MODKEY, KEY, state_seltag, {.ui = 1 << TAG}},                               \
      {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},
/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                                             \
  {                                                                            \
    .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL }                       \
  }

static const Key keys[] = {
    /* modifier              key        function          argument */
    {MODKEY, XK_Return, spawn, {.v = tabtermcmd}},
    //    {MODKEY, XK_p, spawn, {.v = dmenucmd}},
    {MODKEY, XK_b, togglebar, {0}},
    {MODKEY, XK_j, focusstack, {.i = +1}},
    {MODKEY, XK_k, focusstack, {.i = -1}},
    {MODKEY, XK_i, incnmaster, {.i = +1}},
    {MODKEY, XK_d, incnmaster, {.i = -1}},
    {MODKEY, XK_Left, setmfact, {.f = -0.05}},
    {MODKEY, XK_Right, setmfact, {.f = +0.05}},
    {MODKEY, XK_space, setlayout, {0}},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}},
    {MODKEY, XK_c, killclient, {0}},
    {MODKEY | ShiftMask, XK_q, quit, {0}},
    /* extra stuff */
    {MODKEY | ShiftMask, XK_Return, spawn,
     SHCMD("$HOME/.local/bin/st-samedir")},
    {MODKEY, XK_e, spawn, SHCMD("st -e nvim")},
    {0,
     XK_F4,
     spawn,
     {.v = (const char *[]){"amixer", "set", "Capture", "toggle", NULL}}},
    {MODKEY, XK_n, spawn,
     SHCMD("st -t notes -e nvim $HOME/sync/docs/notes/quicknotes.md")},
    {MODKEY | ShiftMask, XK_f, spawn, SHCMD("firefox")},
    /* Function keys */
    {0, XF86XK_AudioMute, spawn, SHCMD("amixer set Master toggle")},
    {0, XF86XK_AudioRaiseVolume, spawn, SHCMD("amixer set Master 1%+")},
    {0, XF86XK_AudioLowerVolume, spawn, SHCMD("amixer set Master 1%-")},
    //    { 0, XF86XK_AudioMicMute, spawn, SHCMD("amixer set Capture toggle") },
    {0, XF86XK_MonBrightnessUp, spawn, SHCMD("brightnessctl set +1%")},
    {0, XF86XK_MonBrightnessDown, spawn, SHCMD("brightnessctl set 1%-")},
    /* D menooo */
    {Mod1Mask, XK_F12, spawn,
     SHCMD("$HOME/.local/bin/display-init && $HOME/.local/bin/toggle-display")},
    {Mod1Mask, XK_space, spawn, {.v = dmenucmd}},
    {Mod1Mask, XK_b, spawn, SHCMD("$HOME/.local/bin/menu/surfmenu")},
    {Mod1Mask, XK_f, spawn, SHCMD("st -e lf")},
    {Mod1Mask, XK_m, spawn, SHCMD("$HOME/.local/bin/menu/music/musicmenu")},
    {Mod1Mask, XK_p, spawn, SHCMD("$HOME/.local/bin/menu/pomodmenu")},
    {0, XK_Print, spawn, SHCMD("$HOME/.local/bin/menu/scrshotmenu")},
    {Mod1Mask, XK_Escape, spawn, SHCMD("$HOME/.local/bin/menu/powermenu")},
    {Mod1Mask, XK_F10, spawn, SHCMD("$HOME/.local/bin/menu/connectmenu")},
    {Mod1Mask, XK_Super_L, spawn, SHCMD("$HOME/.local/bin/menu/allmenu")},
    /* namespace switching */
    {MODKEY, XK_F1, switchns, {.i = 0}},
    {MODKEY, XK_F2, switchns, {.i = 1}},
    {MODKEY, XK_F3, switchns, {.i = 2}},
    /* tags 1-6 */
    TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
        TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5)};

/* ── mouse buttons ────────────────────────────────────────────────────────
 */

static const Button buttons[] = {
    {ClkTagBar, 0, Button1, state_seltag, {0}},
    {ClkTagBar, 0, Button3, toggletag, {0}},
    {ClkWinTitle, 0, Button2, zoom, {0}},
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
};

#endif /* CONFIG_H */
