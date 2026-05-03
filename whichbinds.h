/* whichbinds.h – muhhwm which‑key command tree (original engine, no action/arg)
 */

#ifndef WHICHBINDS_H
#define WHICHBINDS_H
#include <X11/XF86keysym.h>
#include <X11/keysym.h>

/* ── data structure (unchanged) ─────────────────────────────────────────── */
typedef struct WhichKey WhichKey;

struct WhichKey {
  KeySym key;
  const char *label;
  const char *cmd;    /* NULL if this is a prefix */
  WhichKey *children; /* NULL if this is a leaf   */
  int nchildren;
};

#define WK_LEAF(k, lbl, command)                                               \
  {.key = (k),                                                                 \
   .label = (lbl),                                                             \
   .cmd = (command),                                                           \
   .children = NULL,                                                           \
   .nchildren = 0}

#define WK_PREFIX(k, lbl, submenu)                                             \
  {.key = (k),                                                                 \
   .label = (lbl),                                                             \
   .cmd = NULL,                                                                \
   .children = (submenu),                                                      \
   .nchildren = (int)(sizeof(submenu) / sizeof(submenu[0]))}

/* ── workspace via xdotool (with small delay) ──────────────────────────── */
static WhichKey wk_workspace_ns1[] = {
    WK_LEAF('1', "β",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+1'"),
    WK_LEAF('2', "β",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+2'"),
    WK_LEAF('3', "γ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+3'"),
    WK_LEAF('4', "δ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+4'"),
    WK_LEAF('5', "ε",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+5'"),
    WK_LEAF('6', "ζ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F1; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+6'"),
};

static WhichKey wk_workspace_ns2[] = {
    WK_LEAF('1', "α",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+1'"),
    WK_LEAF('2', "β",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+2'"),
    WK_LEAF('3', "γ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+3'"),
    WK_LEAF('4', "δ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+4'"),
    WK_LEAF('5', "ε",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+5'"),
    WK_LEAF('6', "ζ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F2; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+6'"),
};

static WhichKey wk_workspace_ns3[] = {
    WK_LEAF('1', "α",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+1'"),
    WK_LEAF('2', "β",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+2'"),
    WK_LEAF('3', "γ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+3'"),
    WK_LEAF('4', "δ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+4'"),
    WK_LEAF('5', "ε",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+5'"),
    WK_LEAF('6', "ζ",
            "sh -c 'xdotool key --clearmodifiers Super_L+F3; sleep 0.1; "
            "xdotool key --clearmodifiers Super_L+6'"),
};

static WhichKey wk_workspace[] = {
    WK_PREFIX('1', "study", wk_workspace_ns1),
    WK_PREFIX('2', "code", wk_workspace_ns2),
    WK_PREFIX('3', "free", wk_workspace_ns3),
};

/* ── rest of your menus, only mod is adding workspace under Open ─────────── */
static WhichKey wk_nvim[] = {
    WK_LEAF(XK_Return, "nvim", "st -e nvim"),
    WK_LEAF('n', "quicknotes",
            "st -e nvim $HOME/sync/docs/notes/quicknotes.md"),
};

static WhichKey wk_games_linux[] = {
    WK_LEAF('t', "terraria", "/sdc/ROMs/linux/Terraria/terraria.sh"),
    WK_LEAF('s', "slay the princess",
            "st -e /sdc/ROMs/linux/Slay The Princess/slay_the_princess.sh"),
    WK_LEAF('k', "kingdom-and-castle",
            "st -e /sdc/ROMs/linux/Kingdoms and Castles/kingdom-and-castle.sh"),
    WK_LEAF('d', "DDLC", "/sdc/ROMs/linux/DDLC-1.1.1-pc/DDLC.sh"),
    WK_LEAF('D', "DELTARUNE", "/sdc/ROMs/linux/DELTARUNE/DELTARUNE.sh"),
    WK_LEAF('b', "buckshot_roulette",
            "st -e /sdc/ROMs/linux/Buckshot Roulette/buckshot_roulette.sh"),
    WK_LEAF('c', "chess-5D", "/sdc/ROMs/linux/chess-5D/chess-5D.sh"),
};

static WhichKey wk_games[] = {
    WK_PREFIX('l', "linux+", wk_games_linux),
    WK_LEAF('r', "ROMs", "st -e rom"),
};

static WhichKey wk_apps[] = {
    WK_LEAF('f', "firefox", "firefox"),
    WK_LEAF('F', "files", "pcmanfm"),
    WK_LEAF('l', "logseq", "$HOME/.local/bin/logseq.AppImage"),
};

static WhichKey wk_open[] = {
    WK_PREFIX('v', "nvim+", wk_nvim),
    WK_LEAF('t', "terminal", "tabbed -r 2 st -w ''"),
    WK_LEAF('m', "music", "$HOME/.local/bin/menu/music/musicmenu"),
    WK_PREFIX('g', "games+", wk_games),
    WK_PREFIX('w', "workspace+", wk_workspace), /* ← new */
    WK_PREFIX('a', "apps+", wk_apps)};

static WhichKey wk_power[] = {
    WK_LEAF('l', "lock", "slock"),
    WK_LEAF('s', "suspend", "systemctl suspend"),
    WK_LEAF('r', "reboot", "systemctl reboot"),
    WK_LEAF('q', "shutdown", "systemctl poweroff"),
    WK_LEAF('o', "logout", "pkill -9 muhhwm"),
};

static WhichKey wk_settings[] = {
    WK_LEAF('t', "theme", "$HOME/.local/bin/menu/themenu"),
    WK_LEAF('f', "font", "$HOME/.local/bin/menu/fontmenu"),
    WK_LEAF('m', "muhhwm", "st -e nvim $HOME/.local/src/muhhwm/"),
    WK_LEAF('w', "wi-fi", "connectmenu --wifi"),
    WK_LEAF('b', "bluetooth", "connectmenu --bluetooth"),
    WK_LEAF('W', "live wallp", "muhhweatherwall &"),
};

static WhichKey wk_system[] = {
    WK_PREFIX('p', "power+", wk_power),
    WK_PREFIX('s', "settings+", wk_settings),
};

static WhichKey wk_root[] = {
    WK_PREFIX('s', "system+", wk_system),
    WK_PREFIX('o', "open+", wk_open),
    WK_LEAF(XK_space, "panl", "muhhtoggle"),
};

#endif /* WHICHBINDS_H */
