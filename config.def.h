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

/* ── Live wallpaper master switch ── */
static const int wallpaper_enable = 1;
static const int wallpaper_default_mode =
    1; /* 1 = weather scenery at startup */

/* Foreground / background used ONLY by Conway, Starfield, RD, Boids,
   Rain‑manual. The weather scenery has its own built‑in palette and ignores
   these. */
static const char *wallpaper_fg = COL_ACCENT;
static const char *wallpaper_bg = COL_BG;

/* ── Conway’s Game of Life ── */
static const int wallpaper_conway_cellsize = 4;
static const int wallpaper_conway_interval = 250;
static const int wallpaper_conway_density = 20;

/* ── Starfield ── */
static const int wallpaper_stars_count = 300;
static const int wallpaper_stars_interval = 40;

/* ── Reaction‑Diffusion ── */
static const int wallpaper_rd_cellsize = 4;
static const int wallpaper_rd_interval = 40;
static const float wallpaper_rd_feed = 0.036f;
static const float wallpaper_rd_kill = 0.062f;

/* ── Boids ── */
static const int wallpaper_boids_count = 40;
static const int wallpaper_boids_interval = 30;

/* ── Rain (manual) ── */
static const int wallpaper_rain_count = 200;
static const int wallpaper_rain_interval = 30;

/* ── Static wallpaper fallback ── */
static const char *static_wallpaper_cmd[] = {
    "/bin/sh", "-c", "feh --bg-fill --randomize $HOME/.local/share/wallpapers/",
    NULL};

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

/* top-edge interactive bar */
static const int top_edge_zone = 4;               /* idle height (px) */
static const int top_edge_expanded = 20;          /* hover height (px) */
static const int top_edge_bg = 0x1a1b26;          /* thin bar background */
static const int top_edge_expanded_bg = 0x2e3440; /* expanded background */
static const int island_bg = 0x88c0d0;            /* island accent */
static const int top_edge_text_color = 0xd8dee9;  /* text foreground */

/* ── bindings ───────────────────────────────────────────────────────────── */

#include "binds.h"

#endif /* CONFIG_H */
