
/* binds.h - muhhwm keybindings and mouse buttons
 *
 * modifier keys:
 *   MODKEY       = Super (Win key)
 *   Mod1Mask     = Alt
 *   ShiftMask    = Shift
 *   ControlMask  = Ctrl
 *
 * tag navigation:
 *   mod+h/l          prev/next tag
 *   mod+shift+h/l    prev/next namespace
 *   mod+j/k          move window to prev/next tag
 *   mod+shift+j/k    move window to prev/next namespace
 *   mod+1-6          jump to tag N
 *   mod+shift+1-6    move window to tag N
 *   mod+F1/F2/F3     jump to namespace study/code/free
 */

#ifndef BINDS_H
#define BINDS_H

/* ── modifier ───────────────────────────────────────────────────────────── */

#include <X11/X.h>
#define MODKEY Mod4Mask

/* ── helpers ────────────────────────────────────────────────────────────── */

/* mod+N = view tag N, mod+shift+N = move window to tag N */
#define TAGKEYS(KEY, TAG)                                                      \
  {MODKEY, KEY, state_seltag, {.ui = 1 << TAG}},                               \
      {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},

/* spawn a shell command */
#define SHCMD(cmd)                                                             \
  {                                                                            \
    .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL }                       \
  }

/* ── commands ───────────────────────────────────────────────────────────── */

static const char *dmenucmd[] = {"dmenu_run", NULL};
static const char *tabtermcmd[] = {"tabbed", "-r 2", "st", "-w", "''", NULL};

/* ── keybindings ────────────────────────────────────────────────────────── */

static const Key keys[] = {

    /* ── windows ───────────────────────────────────────────────────────── */
    {MODKEY, XK_Return, spawn, {.v = tabtermcmd}},
    {MODKEY | ShiftMask, XK_Return, spawn,
     SHCMD("$HOME/.local/bin/st-samedir")},
    {MODKEY, XK_space, whichkey, {0}},
    {MODKEY, XK_n, barnotes, {0}},
    {MODKEY, XK_c, killclient, {0}},
    {MODKEY, XK_Alt_L, setlayout, {0}},
    {MODKEY | ShiftMask, XK_Alt_L, togglefloating, {0}},
    {MODKEY, XK_b, toggleboth, {0}},
    {MODKEY | ShiftMask, XK_q, quit, {0}},
    /* ── navigation ─────────────────────────────────────────────────── */
    {MODKEY, XK_h, viewadjacent, {.i = -1}},
    {MODKEY, XK_l, viewadjacent, {.i = +1}},
    {MODKEY, XK_j, viewadjacentns, {.i = -1}},
    {MODKEY, XK_k, viewadjacentns, {.i = +1}},

    /* ── shifting ───────────────────────────────────────────── */
    {MODKEY | ShiftMask, XK_h, tagadjacent, {.i = -1}},
    {MODKEY | ShiftMask, XK_l, tagadjacent, {.i = +1}},
    {MODKEY | ShiftMask, XK_j, tagadjacentns, {.i = -1}},
    {MODKEY | ShiftMask, XK_k, tagadjacentns, {.i = +1}},
    {MODKEY, XK_F1, switchns, {.i = 0}},
    {MODKEY, XK_F2, switchns, {.i = 1}},
    {MODKEY, XK_F3, switchns, {.i = 2}},

    /* ── apps ──────────────────────────────────────────────────────────── */
    {MODKEY | ShiftMask, XK_f, spawn, SHCMD("firefox")},

    /* ── dmenu scripts ──────────────────────────────────────────────────── */
    {Mod1Mask, XK_space, spawn, {.v = dmenucmd}},
    {Mod1Mask, XK_b, spawn, SHCMD("$HOME/.local/bin/menu/surfmenu")},
    {Mod1Mask, XK_f, spawn, SHCMD("st -e lf")},
    {Mod1Mask, XK_m, spawn, SHCMD("$HOME/.local/bin/menu/music/musicmenu")},
    {Mod1Mask, XK_p, spawn, SHCMD("$HOME/.local/bin/menu/pomodmenu")},
    {Mod1Mask, XK_Escape, spawn, SHCMD("$HOME/.local/bin/menu/powermenu")},
    {Mod1Mask, XK_F10, spawn, SHCMD("$HOME/.local/bin/menu/connectmenu")},
    {Mod1Mask, XK_Super_L, spawn, SHCMD("$HOME/.local/bin/menu/allmenu")},
    {Mod1Mask, XK_F12, spawn,
     SHCMD("$HOME/.local/bin/display-init && $HOME/.local/bin/toggle-display")},

    /* ── media ──────────────────────────────────────────────────────────── */
    {0, XF86XK_AudioMute, spawn, SHCMD("amixer set Master toggle")},
    {0, XF86XK_AudioRaiseVolume, spawn, SHCMD("amixer set Master 1%+")},
    {0, XF86XK_AudioLowerVolume, spawn, SHCMD("amixer set Master 1%-")},
    {0, XF86XK_MonBrightnessUp, spawn, SHCMD("brightnessctl set +1%")},
    {0, XF86XK_MonBrightnessDown, spawn, SHCMD("brightnessctl set 1%-")},
    {0,
     XK_F4,
     spawn,
     {.v = (const char *[]){"amixer", "set", "Capture", "toggle", NULL}}},
    {0, XK_Print, spawn, SHCMD("$HOME/.local/bin/menu/scrshotmenu")},

    /* ── tags 1-6 ───────────────────────────────────────────────────────── */
    TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
        TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5)};

/* ── mouse buttons ──────────────────────────────────────────────────────── */

static const Button buttons[] = {
    /* click area      mask    button   function       arg */
    {ClkTagBar, 0, Button1, state_seltag, {0}}, /* click tag = view it */
    {ClkTagBar, 0, Button3, toggletag, {0}}, /* right click tag = toggle     */
    {ClkWinTitle, 0, Button2, zoom, {0}},    /* middle click title = zoom    */
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
    {ClkTagBar, 0, 6, viewadjacent, {.i = -1}},
    {ClkTagBar, 0, 7, viewadjacent, {.i = +1}},
    {ClkTagBar, 0, Button4, viewadjacentns, {.i = -1}},
};

#endif /* BINDS_H */
