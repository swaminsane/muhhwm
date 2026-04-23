/* bar.c - muhhwm bar drawing */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "../whichbinds.h"
#include "config.h"
#include "muhh.h"

static int bh(void) { return wm.drw->fonts->h + 2; }

/* ── which-key state ─────────────────────────────────────────────────────── */

static int wk_active = 0;
static WhichKey *wk_cur = NULL;
static int wk_ncur = 0;

static void wk_grab_all(void) {
  XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
  XGrabKey(wm.dpy, AnyKey, AnyModifier, wm.root, True, GrabModeAsync,
           GrabModeAsync);
}

static void wk_ungrab_all(void) {
  XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
  x11_grabkeys();
}

void bar_whichkey_activate(void) {
  wk_active = 1;
  wk_cur = wk_root;
  wk_ncur = (int)(sizeof(wk_root) / sizeof(wk_root[0]));
  wk_grab_all();
  for (Monitor *m = wm.mons; m; m = m->next)
    bar_draw(m);
}

int bar_whichkey_active(void) { return wk_active; }

static void wk_close(void) {
  wk_active = 0;
  wk_cur = NULL;
  wk_ncur = 0;
  wk_ungrab_all();
  for (Monitor *m = wm.mons; m; m = m->next)
    bar_draw(m);
}

void bar_whichkey_key(KeySym ks) {
  if (ks == XK_Escape) {
    wk_close();
    return;
  }

  for (int i = 0; i < wk_ncur; i++) {
    if (wk_cur[i].key == ks) {
      if (wk_cur[i].cmd) {
        const char *cmd[] = {"/bin/sh", "-c", wk_cur[i].cmd, NULL};
        Arg a = {.v = cmd};
        spawn(&a);
        wk_close();
      } else {
        WhichKey *next = wk_cur[i].children;
        int nnext = wk_cur[i].nchildren;
        wk_cur = next;
        wk_ncur = nnext;
        for (Monitor *m = wm.mons; m; m = m->next)
          bar_draw(m);
      }
      return;
    }
  }
}

/* ── notes state ─────────────────────────────────────────────────────────── */

static int nt_active = 0;               /* notes mode on/off          */
static char nt_buf[NOTES_MAXLEN];       /* current input buffer    */
static int nt_len = 0;                  /* length of text in buffer   */
static int nt_cursor = 0;               /* cursor position            */
static int nt_view = 0;                 /* scroll offset (chars)      */
static int nt_chaining = 0;             /* 1 if previous line ended \ */
static char nt_chain_buf[NOTES_MAXLEN]; /* first line when chaining */

/* get current date string "2026-04-22" */
static void nt_date(char *out, int outlen) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(out, (size_t)outlen, "%Y-%m-%d", tm);
}

/* get current time string "14:32" */
static void nt_time(char *out, int outlen) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(out, (size_t)outlen, "%H:%M", tm);
}

/* get tag number of current focused window (1-based) */
static int nt_current_tag(void) {
  Namespace *ns = SELNS();
  return __builtin_ctz(ns->tagset[ns->seltags]) + 1;
}

/* get focused window title */
static const char *nt_wintitle(void) {
  Namespace *ns = SELNS();
  if (ns->sel)
    return ns->sel->name;
  return "";
}

/* check if last date header in file matches today, append if not */
static void nt_ensure_date_header(FILE *f, const char *date) {
  /* seek to find last date header — simplest: just write if file is new */
  /* rewind and scan last line for date */
  char line[256];
  char lastdate[32] = "";
  rewind(f);
  while (fgets(line, sizeof(line), f)) {
    /* date headers look like "## 2026-04-22" */
    if (line[0] == '#' && line[1] == '#' && line[2] == ' ')
      strncpy(lastdate, line + 3, sizeof(lastdate) - 1);
  }
  /* strip newline from lastdate */
  lastdate[strcspn(lastdate, "\n")] = '\0';

  if (strcmp(lastdate, date) != 0) {
    /* new day — append date header */
    fprintf(f, "\n## %s\n\n", date);
  }
}

/* save current buffer as a note entry */
static void nt_save(void) {
  if (nt_len == 0)
    return;

  char date[32], ts[16];
  nt_date(date, sizeof(date));
  nt_time(ts, sizeof(ts));

  int ns_idx = wm.selmon->ans;
  int tag = nt_current_tag();
  const char *wintitle = nt_wintitle();
  const char *nsname = nsnames[ns_idx];

  FILE *f = fopen(notes_file, "a+");
  if (!f)
    return;

  nt_ensure_date_header(f, date);

  if (nt_chaining) {
    fprintf(f, "  %s\n", nt_buf);
  } else {
    fprintf(f, "[%s] [%s/%d] [%s] %s\n", ts, nsname, tag, wintitle, nt_buf);
  }

  fclose(f);
}

/* open notes file in nvim at last line */
static void nt_open_in_nvim(void) {
  char cmd[600];
  snprintf(cmd, sizeof(cmd), "st -e nvim + %s", notes_file);
  const char *argv[] = {"/bin/sh", "-c", cmd, NULL};
  Arg a = {.v = argv};
  spawn(&a);
}

static void nt_grab(void) {
  XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
  XGrabKey(wm.dpy, AnyKey, AnyModifier, wm.root, True, GrabModeAsync,
           GrabModeAsync);
}

static void nt_ungrab(void) {
  XUngrabKey(wm.dpy, AnyKey, AnyModifier, wm.root);
  x11_grabkeys();
}

static void nt_redraw(void) {
  for (Monitor *m = wm.mons; m; m = m->next)
    bar_draw(m);
}

static void nt_close(void) {
  nt_active = 0;
  nt_len = 0;
  nt_cursor = 0;
  nt_view = 0;
  nt_chaining = 0;
  nt_buf[0] = '\0';
  nt_ungrab();
  nt_redraw();
}

void bar_selection_notify(XEvent *e) {
  XSelectionEvent *ev = &e->xselection;
  if (ev->property == None)
    return;

  Atom type;
  int format;
  unsigned long nitems, after;
  unsigned char *data = NULL;

  XGetWindowProperty(wm.dpy, wm.root, ev->property, 0, 4096, True,
                     AnyPropertyType, &type, &format, &nitems, &after, &data);
  if (data) {
    char *p = (char *)data;
    while (*p && nt_len < NOTES_MAXLEN - 1) {
      if (*p >= 32 && *p <= 126) {
        memmove(nt_buf + nt_cursor + 1, nt_buf + nt_cursor,
                (size_t)(nt_len - nt_cursor + 1));
        nt_buf[nt_cursor] = *p;
        nt_cursor++;
        nt_len++;
      }
      p++;
    }
    XFree(data);
    for (Monitor *m = wm.mons; m; m = m->next)
      bar_draw(m);
  }
}

void bar_notes_activate(void) {
  if (nt_active)
    return;
  nt_active = 1;
  nt_len = 0;
  nt_cursor = 0;
  nt_view = 0;
  nt_chaining = 0;
  nt_buf[0] = '\0';
  nt_grab();
  nt_redraw();
}

int bar_notes_active(void) { return nt_active; }

void bar_notes_key(KeySym ks, unsigned int state) {
  int ctrl = (state & ControlMask) != 0;

  /* ── Escape — cancel ── */
  if (ks == XK_Escape) {
    nt_chaining = 0;
    nt_close();
    return;
  }

  /* ── Ctrl+o — open file in nvim ── */
  if (ctrl && ks == XK_o) {
    nt_open_in_nvim();
    nt_close();
    return;
  }

  /* ── Enter — save ── */
  if (ks == XK_Return) {
    if (nt_len == 0) {
      /* empty note — do nothing, stay open */
      return;
    }
    /* check for chain continuation: ends with \ */
    if (nt_len >= 1 && nt_buf[nt_len - 1] == '\\') {
      /* save first line, enter chaining mode */
      nt_save();
      strncpy(nt_chain_buf, nt_buf, sizeof(nt_chain_buf) - 1);
      nt_buf[0] = '\0';
      nt_len = 0;
      nt_cursor = 0;
      nt_view = 0;
      nt_chaining = 1;
      nt_redraw();
      return;
    }
    nt_save();
    nt_chaining = 0;
    nt_close();
    return;
  }

  /* ── Ctrl+u — clear line ── */
  if (ctrl && ks == XK_u) {
    nt_buf[0] = '\0';
    nt_len = 0;
    nt_cursor = 0;
    nt_view = 0;
    nt_redraw();
    return;
  }

  /* ── Ctrl+w — delete last word ── */
  if (ctrl && ks == XK_w) {
    /* eat trailing spaces */
    while (nt_cursor > 0 && nt_buf[nt_cursor - 1] == ' ') {
      memmove(nt_buf + nt_cursor - 1, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      nt_cursor--;
      nt_len--;
    }
    /* eat word chars */
    while (nt_cursor > 0 && nt_buf[nt_cursor - 1] != ' ') {
      memmove(nt_buf + nt_cursor - 1, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      nt_cursor--;
      nt_len--;
    }
    nt_redraw();
    return;
  }

  /* ── Ctrl+a — start of line ── */
  if (ctrl && ks == XK_a) {
    nt_cursor = 0;
    nt_view = 0;
    nt_redraw();
    return;
  }

  /* ── Ctrl+e — end of line ── */
  if (ctrl && ks == XK_e) {
    nt_cursor = nt_len;
    nt_redraw();
    return;
  }

  /* ── Left / Ctrl+Left ── */
  if (ks == XK_Left) {
    if (ctrl) {
      /* move one word left */
      while (nt_cursor > 0 && nt_buf[nt_cursor - 1] == ' ')
        nt_cursor--;
      while (nt_cursor > 0 && nt_buf[nt_cursor - 1] != ' ')
        nt_cursor--;
    } else {
      if (nt_cursor > 0)
        nt_cursor--;
    }
    nt_redraw();
    return;
  }

  /* ── Right / Ctrl+Right ── */
  if (ks == XK_Right) {
    if (ctrl) {
      while (nt_cursor < nt_len && nt_buf[nt_cursor] != ' ')
        nt_cursor++;
      while (nt_cursor < nt_len && nt_buf[nt_cursor] == ' ')
        nt_cursor++;
    } else {
      if (nt_cursor < nt_len)
        nt_cursor++;
    }
    nt_redraw();
    return;
  }

  /* ── Backspace ── */
  if (ks == XK_BackSpace) {
    if (nt_cursor > 0) {
      memmove(nt_buf + nt_cursor - 1, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      nt_cursor--;
      nt_len--;
      nt_redraw();
    }
    return;
  }

  /* ── Ctrl+l — insert #link ── */
  if (ctrl && ks == XK_l) {
    const char *tag = "#link []()";
    int tlen = (int)strlen(tag);
    if (nt_len + tlen < NOTES_MAXLEN - 1) {
      memmove(nt_buf + nt_cursor + tlen, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      memcpy(nt_buf + nt_cursor, tag, (size_t)tlen);
      nt_cursor += tlen - 3; /* place cursor inside [] */
      nt_len += tlen;
      nt_redraw();
    }
    return;
  }

  /* ── Ctrl+q — insert #todo ── */
  if (ctrl && ks == XK_q) {
    const char *tag = "#todo ";
    int tlen = (int)strlen(tag);
    if (nt_len + tlen < NOTES_MAXLEN - 1) {
      memmove(nt_buf + nt_cursor + tlen, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      memcpy(nt_buf + nt_cursor, tag, (size_t)tlen);
      nt_cursor += tlen;
      nt_len += tlen;
      nt_redraw();
    }
    return;
  }

  /* ── Ctrl+i — insert #idea ── */
  if (ctrl && ks == XK_i) {
    const char *tag = "#idea ";
    int tlen = (int)strlen(tag);
    if (nt_len + tlen < NOTES_MAXLEN - 1) {
      memmove(nt_buf + nt_cursor + tlen, nt_buf + nt_cursor,
              (size_t)(nt_len - nt_cursor + 1));
      memcpy(nt_buf + nt_cursor, tag, (size_t)tlen);
      nt_cursor += tlen;
      nt_len += tlen;
      nt_redraw();
    }
    return;
  }

  /* ── Ctrl+v — paste from clipboard (xclip needed) ── */
  if (ctrl && ks == XK_v) {
    FILE *p = popen(
        "xclip -selection clipboard -o 2>/dev/null || xsel -bo 2>/dev/null",
        "r");
    if (p) {
      char ch;
      while ((ch = fgetc(p)) != EOF && nt_len < NOTES_MAXLEN - 1) {
        if (ch >= 32 && ch <= 126) {
          memmove(nt_buf + nt_cursor + 1, nt_buf + nt_cursor,
                  (size_t)(nt_len - nt_cursor + 1));
          nt_buf[nt_cursor] = ch;
          nt_cursor++;
          nt_len++;
        }
      }
      pclose(p);
      nt_redraw();
    }
    return;
  }

  /* ── printable character — insert at cursor ── */
  if (ks >= 32 && ks <= 126) {
    char ch = (char)ks;
    if (nt_len >= NOTES_MAXLEN - 1)
      return;
    memmove(nt_buf + nt_cursor + 1, nt_buf + nt_cursor,
            (size_t)(nt_len - nt_cursor + 1));
    nt_buf[nt_cursor] = ch;
    nt_cursor++;
    nt_len++;
    nt_redraw();
  }
}

static int muhhbar_width(void) {
  Atom a = XInternAtom(wm.dpy, "_MUHHBAR_WIDTH", False);
  Atom type;
  int format;
  unsigned long n, after;
  unsigned char *data = NULL;
  int w = 0;
  if (XGetWindowProperty(wm.dpy, wm.root, a, 0, 1, False, XA_CARDINAL, &type,
                         &format, &n, &after, &data) == Success &&
      data) {
    w = (int)*(unsigned long *)data;
    XFree(data);
  }
  return w;
}

/* ── bar_init ──────────────────────────────────────────────────────────────
 */

void bar_init(Monitor *m) {
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"muhhwm", "muhhwm"};
  int h = bh();

  m->bar.h = h;
  m->bar.showbar = showbar;
  m->bar.topbar = topbar;

  m->wh = m->mh;
  if (m->bar.showbar) {
    m->wh -= h;
    m->bar.y = m->bar.topbar ? m->my : m->my + m->wh;
    if (m->bar.topbar)
      m->wy = m->my + h;
  } else {
    m->bar.y = -h;
  }
  m->bar.w = m->mw;

  if (m->bar.win)
    return;

  m->bar.win =
      XCreateWindow(wm.dpy, wm.root, m->wx, m->bar.y, m->bar.w, h, 0,
                    DefaultDepth(wm.dpy, wm.screen), CopyFromParent,
                    DefaultVisual(wm.dpy, wm.screen),
                    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
  XDefineCursor(wm.dpy, m->bar.win, wm.cursor[CurNormal]);
  XMapRaised(wm.dpy, m->bar.win);
  XSetClassHint(wm.dpy, m->bar.win, &ch);
}

/* ── bar_draw ──────────────────────────────────────────────────────────────
 */

void bar_draw(Monitor *m) {
  int x, w, tw = 0;
  int boxs = wm.drw->fonts->h / 9;
  int boxw = wm.drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  int h = bh();
  Namespace *ns = &wm.ns[m->ans];
  Client *c;

  if (!m->bar.showbar)
    return;

  /* status text — only on selected monitor */
  if (m == wm.selmon) {
    drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
    tw = TEXTW(stext) - wm.lrpad + 2;
    drw_text(wm.drw, m->bar.w - tw, 0, tw, h, 0, stext, 0);
  }

  /* namespace indicator */
  drw_setscheme(wm.drw, wm.nsscheme[m->ans]);
  w = TEXTW(nsnames[m->ans]);
  drw_text(wm.drw, 0, 0, w, h, wm.lrpad / 2, nsnames[m->ans], 0);
  x = w;

  /* tags */
  for (c = ns->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  for (i = 0; i < NTAGS; i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(
        wm.drw,
        wm.scheme[ns->tagset[ns->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      drw_rect(wm.drw, x + boxs, boxs, boxw, boxw,
               m == wm.selmon && ns->sel && ns->sel->tags & 1 << i,
               urg & 1 << i);
    x += w;
  }

  /* layout symbol */
  Tag *ct = CURTAG(ns);
  w = TEXTW(ct->lt[ct->sellt]->symbol);
  drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
  x = drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, ct->lt[ct->sellt]->symbol, 0);

  /* title area */
  int mbw = muhhbar_width();
  if ((w = m->bar.w - tw - x - mbw) > h) {
    if (wk_active) {
      /* which-key mode */
      drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
      drw_rect(wm.drw, x, 0, w, h, 1, 1);
      int hx = x;
      for (i = 0; i < (unsigned int)wk_ncur; i++) {
        char hint[64];
        const char *ksname = XKeysymToString(wk_cur[i].key);
        snprintf(hint, sizeof(hint), " [%s] %s ", ksname ? ksname : "?",
                 wk_cur[i].label);
        int hw = TEXTW(hint);
        if (hx + hw > m->bar.w - tw)
          break;
        drw_setscheme(wm.drw, wm.scheme[i % 2 == 0 ? SchemeNorm : SchemeSel]);
        drw_text(wm.drw, hx, 0, hw, h, 0, hint, 0);
        hx += hw;
      }
    } else if (nt_active) {
      /* notes mode */
      drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
      drw_rect(wm.drw, x, 0, w, h, 1, 1);

      /* prefix: show "> " or "+ " when chaining */
      char prefix[16];
      snprintf(prefix, sizeof(prefix), nt_chaining ? " + " : " > ");
      int px = TEXTW(prefix);
      drw_setscheme(wm.drw, wm.scheme[SchemeSel]);
      drw_text(wm.drw, x, 0, px, h, 0, prefix, 0);

      /* calculate visible window into buffer */
      int text_area = w - px - wm.lrpad * 2;
      /* adjust view so cursor is always visible */
      /* measure chars from view offset until we exceed text_area */
      int view_end = nt_view;
      int used = 0;
      while (view_end < nt_len) {
        char tmp[2] = {nt_buf[view_end], '\0'};
        int cw = TEXTW(tmp) - wm.lrpad;
        if (used + cw > text_area)
          break;
        used += cw;
        view_end++;
      }
      /* scroll right if cursor past view_end */
      if (nt_cursor > view_end) {
        nt_view = nt_cursor - (view_end - nt_view) + 1;
        if (nt_view > nt_len)
          nt_view = nt_len;
      }
      /* scroll left if cursor before view */
      if (nt_cursor < nt_view)
        nt_view = nt_cursor;

      /* draw visible slice */
      char visible[NOTES_MAXLEN];
      int vlen = 0;
      int vpos = nt_view;
      used = 0;
      while (vpos < nt_len) {
        char tmp[2] = {nt_buf[vpos], '\0'};
        int cw = TEXTW(tmp) - wm.lrpad;
        if (used + cw > text_area)
          break;
        visible[vlen++] = nt_buf[vpos++];
        used += cw;
      }
      visible[vlen] = '\0';

      drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
      drw_text(wm.drw, x + px, 0, w - px, h, wm.lrpad / 2, visible, 0);

      /* draw cursor as underscore at cursor position */
      int coff = 0;
      for (int j = nt_view; j < nt_cursor; j++) {
        char tmp[2] = {nt_buf[j], '\0'};
        coff += TEXTW(tmp) - wm.lrpad;
      }
      drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
      drw_rect(wm.drw, x + px + wm.lrpad / 2 + coff, 1, 2, h - 2, 1, 0);
      // change the line above to drw_rect(wm.drw, x + px + wm.lrpad / 2 + coff,
      // 1, 8, h - 2, 1, 0); for that thick block cursor, change back to 2 for
      // normal

    } else {
      /* normal mode — window title */
      if (ns->sel) {
        drw_setscheme(wm.drw,
                      wm.scheme[m == wm.selmon ? SchemeSel : SchemeNorm]);
        drw_text(wm.drw, x, 0, w, h, wm.lrpad / 2, ns->sel->name, 0);
        if (ns->sel->isfloating)
          drw_rect(wm.drw, x + boxs, boxs, boxw, boxw, ns->sel->isfixed, 0);
      } else {
        drw_setscheme(wm.drw, wm.scheme[SchemeNorm]);
        drw_rect(wm.drw, x, 0, w, h, 1, 1);
      }
    }
  }

  drw_map(wm.drw, m->bar.win, 0, 0, m->bar.w, h);
}

/* ── bar_click ─────────────────────────────────────────────────────────────
 */

void bar_click(Monitor *m, int ex, int button) {
  unsigned int i, x;
  Namespace *ns = &wm.ns[m->ans];
  Arg arg = {0};
  int click = ClkRootWin;

  x = TEXTW(nsnames[m->ans]);
  if (ex < (int)x) {
    click = ClkLtSymbol;
    goto dispatch;
  }

  for (i = 0; i < NTAGS; i++) {
    x += TEXTW(tags[i]);
    if (ex < (int)x) {
      click = ClkTagBar;
      arg.ui = 1 << i;
      goto dispatch;
    }
  }

  Tag *ct = CURTAG(ns);
  x += TEXTW(ct->lt[ct->sellt]->symbol);
  if (ex < (int)x) {
    click = ClkLtSymbol;
    goto dispatch;
  }

  if (ex > m->bar.w - (int)TEXTW(stext) + wm.lrpad - 2)
    click = ClkStatusText;
  else
    click = ClkWinTitle;

dispatch:
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == (unsigned int)button)
      buttons[i].func(click == ClkTagBar && buttons[i].arg.ui == 0
                          ? &arg
                          : &buttons[i].arg);
}
