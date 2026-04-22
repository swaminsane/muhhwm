/* config.h - muhhwm appearance and behaviour
 * keybindings live in binds.h
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "/home/swaminsane/.config/theme/fonts.h"
#include "colors.h"
#include "src/muhh.h"
#include <X11/XF86keysym.h>

/* ── appearance ─────────────────────────────────────────────────────────── */

static const char *fonts[] = {
    FONT_MAIN, "Noto Color Emoji:pixelsize=11:antialias=true:autohint=true"};
static const char dmenufont[] = FONT_MAIN;
static const unsigned int borderpx = BORDER_PX;
static const unsigned int snap = 22;
static const unsigned int refreshrate = 60;
static const int showbar = 1;
static const int topbar = 0;

static const char *notes_file = "/home/swaminsane/sync/docs/bar_thoughts.md";

static const char *colors[][3] = {
    /*               fg          bg          border      */
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

/* tag labels per namespace — greek letters */
static const char *tags[NTAGS] = {"α", "β", "γ", "δ", "ε", "ζ"};

/* bar accent color per namespace [ns][fg, bg] */
static const char *nscolors[NNAMESPACES][2] = {
    {COL_BG, COL_BLUE},    /* study */
    {COL_BG, COL_GREEN},   /* code  */
    {COL_BG, COL_MAGENTA}, /* free  */
};

/* ── layouts ────────────────────────────────────────────────────────────── */

static const float mfact = 0.55;
static const int nmaster = 1;
static const int resizehints = 0;

static const Layout layouts[] = {
    {"[]=", tile},    /* tiling — default        */
    {"[M]", monocle}, /* monocle — one fullscreen */
    {"><>", NULL},    /* floating                 */
};

/* ── window rules ───────────────────────────────────────────────────────── */

static const Rule rules[] = {
    /* class  instance  title  tags  float  ns */
    {NULL, NULL, NULL, 0, 0, -1}, /* no rules — windows open on active ns/tag */
    /* empty — windows open on active namespace/tag */
};

/* ── bindings ───────────────────────────────────────────────────────────── */

#include "binds.h"

#endif /* CONFIG_H */
