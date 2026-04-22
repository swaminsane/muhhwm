
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

static WhichKey wk_open_nvim[] = {
    WK_LEAF(XK_Return, "nvim", "st -e nvim"),
    WK_LEAF('n', "quicknotes",
            "st -e nvim $HOME/sync/docs/notes/quicknotes.md"),
    WK_LEAF('m', "muhhwm", "st -e nvim $HOME/.local/src/muhhwm/"),
};

static WhichKey wk_open[] = {
    WK_PREFIX('v', "nvim+", wk_open_nvim),
    WK_LEAF('f', "firefox", "firefox"),
    WK_LEAF('F', "lf", "st -e lf"),
    WK_LEAF('t', "terminal", "tabbed -r 2 st -w ''"),
    WK_LEAF('z', "zathura", "zathura"),
    WK_LEAF('m', "music", "$HOME/.local/bin/menu/music/musicmenu"),
};

static WhichKey wk_power[] = {
    WK_LEAF('l', "lock", "slock"),
    WK_LEAF('s', "suspend", "systemctl suspend"),
    WK_LEAF('r', "reboot", "systemctl reboot"),
    WK_LEAF('q', "shutdown", "systemctl poweroff"),
    WK_LEAF('o', "logout", "pkill -9 muhhwm"),
};

static WhichKey wk_root[] = {
    WK_PREFIX('o', "open+", wk_open),
    WK_PREFIX('p', "power+", wk_power),
};

#endif /* WHICHBINDS_H */
