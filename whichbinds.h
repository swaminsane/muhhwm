
/* whichbinds.h - muhhwm which-key command tree
 *
 * To add a new leaf (direct action):
 *   WK_LEAF('x', "label", "shell command")
 *
 * To add a new prefix (opens submenu):
 *   1. Define a new array: static WhichKey wk_mygroup[] = { ... };
 *   2. Add WK_PREFIX('x', "label+", wk_mygroup) to the parent array
 *
 * Labels for prefixes should end with + to signal they go deeper.
 * Commands are passed to /bin/sh -c so full shell syntax works.
 */

#ifndef WHICHBINDS_H
#define WHICHBINDS_H
#include <X11/XF86keysym.h>
#include <X11/keysym.h>

/* ── data structure ──────────────────────────────────────────────────────── */

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

static WhichKey wk_open[] = {
    WK_PREFIX('v', "nvim+", wk_nvim),
    WK_LEAF('f', "firefox", "firefox"),
    WK_LEAF('F', "files", "pcmanfm"),
    WK_LEAF('t', "terminal", "tabbed -r 2 st -w ''"),
    WK_LEAF('m', "music", "$HOME/.local/bin/menu/music/musicmenu"),
    WK_PREFIX('g', "games+", wk_games),
};

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
};

static WhichKey wk_system[] = {
    WK_PREFIX('p', "power+", wk_power),
    WK_PREFIX('s', "settings+", wk_settings),
};

static WhichKey wk_root[] = {WK_PREFIX('s', "system+", wk_system),
                             WK_PREFIX('o', "open+", wk_open),
                             WK_LEAF(XK_space, "panl", "muhhtoggle")};

#endif /* WHICHBINDS_H */
