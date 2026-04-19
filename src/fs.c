/* fs.c - muhhwm FUSE filesystem interface
 * mount point: /tmp/muhh
 *
 * tree:
 *   /active_namespace               r/w  name of active namespace
 *   /focused/                       dir  symlink to focused client
 *   /namespaces/                    dir
 *   /namespaces/<ns>/               dir
 *   /namespaces/<ns>/active         r/w  active tag bitmask (decimal)
 *   /namespaces/<ns>/tags/          dir
 *   /namespaces/<ns>/tags/<n>/      dir  n = 1..NTAGS
 *   /namespaces/<ns>/tags/<n>/layout     r/w  layout symbol
 *   /namespaces/<ns>/tags/<n>/clients/   dir
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/  dir
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/name
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/class
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/geometry  r/w
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/floating  r/w
 *   /namespaces/<ns>/tags/<n>/clients/<class>_0x<xid>/focused   r/w
 */

#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fuse3/fuse.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "muhh.h"
#define MOUNTPOINT "/tmp/muhh"

static pthread_t fs_thread;

/* ── path tokenizer ─────────────────────────────────────────────────────── */

typedef struct {
  char parts[8][128]; /* split path components                */
  int n;              /* number of components                 */
} Path;

static void path_parse(const char *path, Path *p) {
  char buf[512];
  char *tok, *save;
  p->n = 0;
  strncpy(buf, path, sizeof buf - 1);
  buf[sizeof buf - 1] = '\0';
  tok = strtok_r(buf, "/", &save);
  while (tok && p->n < 8) {
    strncpy(p->parts[p->n], tok, 127);
    p->parts[p->n][127] = '\0';
    p->n++;
    tok = strtok_r(NULL, "/", &save);
  }
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static int ns_index_by_name(const char *name) {
  int i;
  for (i = 0; i < NNAMESPACES; i++)
    if (strcmp(wm.ns[i].name, name) == 0)
      return i;
  return -1;
}

static Client *client_by_dirname(const char *dirname) {
  /* dirname: "<class>_0x<winid>" — find last '_' then parse hex */
  const char *p = strrchr(dirname, '_');
  if (!p || p[1] != '0' || p[2] != 'x')
    return NULL;
  unsigned long xid = strtoul(p + 1, NULL, 16);
  if (!xid)
    return NULL;
  int i;
  Client *c;
  for (i = 0; i < NNAMESPACES; i++)
    for (c = wm.ns[i].clients; c; c = c->next)
      if ((unsigned long)c->win == xid)
        return c;
  return NULL;
}

static void client_dirname(Client *c, char *buf, size_t sz) {
  const char *cls = c->class[0] ? c->class : "unknown";
  snprintf(buf, sz, "%s_0x%lx", cls, (unsigned long)c->win);
}

static void fill_stat_dir(struct stat *st) {
  st->st_mode = S_IFDIR | 0755;
  st->st_nlink = 2;
}

static void fill_stat_file(struct stat *st, size_t sz) {
  st->st_mode = S_IFREG | 0644;
  st->st_nlink = 1;
  st->st_size = (off_t)sz;
}

/* ── getattr ────────────────────────────────────────────────────────────── */

static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi) {
  (void)fi;
  memset(st, 0, sizeof *st);

  Path p;
  path_parse(path, &p);

  /* / */
  if (p.n == 0) {
    fill_stat_dir(st);
    return 0;
  }

  /* /active_namespace */
  if (p.n == 1 && strcmp(p.parts[0], "active_namespace") == 0) {
    fill_stat_file(st, NNAMELEN);
    return 0;
  }

  /* /focused */
  if (p.n == 1 && strcmp(p.parts[0], "focused") == 0) {
    fill_stat_dir(st);
    return 0;
  }

  /* /focused/<field> */
  if (p.n == 2 && strcmp(p.parts[0], "focused") == 0) {
    fill_stat_file(st, 256);
    return 0;
  }

  /* /namespaces */
  if (p.n == 1 && strcmp(p.parts[0], "namespaces") == 0) {
    fill_stat_dir(st);
    return 0;
  }

  /* /namespaces/<ns> */
  if (p.n >= 2 && strcmp(p.parts[0], "namespaces") == 0) {
    int ni = ns_index_by_name(p.parts[1]);
    if (ni < 0)
      return -ENOENT;

    if (p.n == 2) {
      fill_stat_dir(st);
      return 0;
    }

    /* /namespaces/<ns>/active */
    if (p.n == 3 && strcmp(p.parts[2], "active") == 0) {
      fill_stat_file(st, 16);
      return 0;
    }

    /* /namespaces/<ns>/tags */
    if (p.n == 3 && strcmp(p.parts[2], "tags") == 0) {
      fill_stat_dir(st);
      return 0;
    }

    if (p.n >= 4 && strcmp(p.parts[2], "tags") == 0) {
      int tagn = atoi(p.parts[3]);
      if (tagn < 1 || tagn > NTAGS)
        return -ENOENT;

      /* /namespaces/<ns>/tags/<n> */
      if (p.n == 4) {
        fill_stat_dir(st);
        return 0;
      }

      /* /namespaces/<ns>/tags/<n>/layout */
      if (p.n == 5 && strcmp(p.parts[4], "layout") == 0) {
        fill_stat_file(st, 16);
        return 0;
      }

      /* /namespaces/<ns>/tags/<n>/clients */
      if (p.n == 5 && strcmp(p.parts[4], "clients") == 0) {
        fill_stat_dir(st);
        return 0;
      }

      if (p.n >= 6 && strcmp(p.parts[4], "clients") == 0) {
        Client *c = client_by_dirname(p.parts[5]);
        if (!c)
          return -ENOENT;

        /* /namespaces/<ns>/tags/<n>/clients/<cdir> */
        if (p.n == 6) {
          fill_stat_dir(st);
          return 0;
        }

        /* /namespaces/<ns>/tags/<n>/clients/<cdir>/<field> */
        if (p.n == 7) {
          const char *f = p.parts[6];
          if (strcmp(f, "name") == 0 || strcmp(f, "class") == 0 ||
              strcmp(f, "geometry") == 0 || strcmp(f, "floating") == 0 ||
              strcmp(f, "focused") == 0) {
            fill_stat_file(st, 256);
            return 0;
          }
          return -ENOENT;
        }
      }
    }
    return -ENOENT;
  }

  return -ENOENT;
}

/* ── readdir ────────────────────────────────────────────────────────────── */

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  Path p;
  path_parse(path, &p);

  /* / */
  if (p.n == 0) {
    filler(buf, "namespaces", NULL, 0, 0);
    filler(buf, "active_namespace", NULL, 0, 0);
    filler(buf, "focused", NULL, 0, 0);
    return 0;
  }

  /* /namespaces */
  if (p.n == 1 && strcmp(p.parts[0], "namespaces") == 0) {
    int i;
    for (i = 0; i < NNAMESPACES; i++)
      filler(buf, wm.ns[i].name, NULL, 0, 0);
    return 0;
  }

  if (p.n >= 2 && strcmp(p.parts[0], "namespaces") == 0) {
    int ni = ns_index_by_name(p.parts[1]);
    if (ni < 0)
      return -ENOENT;

    /* /namespaces/<ns> */
    if (p.n == 2) {
      filler(buf, "active", NULL, 0, 0);
      filler(buf, "tags", NULL, 0, 0);
      return 0;
    }

    /* /namespaces/<ns>/tags */
    if (p.n == 3 && strcmp(p.parts[2], "tags") == 0) {
      int i;
      char tagname[8];
      for (i = 1; i <= NTAGS; i++) {
        snprintf(tagname, sizeof tagname, "%d", i);
        filler(buf, tagname, NULL, 0, 0);
      }
      return 0;
    }

    if (p.n >= 4 && strcmp(p.parts[2], "tags") == 0) {
      int tagn = atoi(p.parts[3]);
      if (tagn < 1 || tagn > NTAGS)
        return -ENOENT;

      /* /namespaces/<ns>/tags/<n> */
      if (p.n == 4) {
        filler(buf, "layout", NULL, 0, 0);
        filler(buf, "clients", NULL, 0, 0);
        return 0;
      }

      /* /namespaces/<ns>/tags/<n>/clients */
      if (p.n == 5 && strcmp(p.parts[4], "clients") == 0) {
        Namespace *ns = &wm.ns[ni];
        Client *c;
        char dirname[128];
        for (c = ns->clients; c; c = c->next) {
          if (!(c->tags & (1 << (tagn - 1))))
            continue;
          client_dirname(c, dirname, sizeof dirname);
          filler(buf, dirname, NULL, 0, 0);
        }
        return 0;
      }

      /* /namespaces/<ns>/tags/<n>/clients/<cdir> */
      if (p.n == 6 && strcmp(p.parts[4], "clients") == 0) {
        filler(buf, "name", NULL, 0, 0);
        filler(buf, "class", NULL, 0, 0);
        filler(buf, "geometry", NULL, 0, 0);
        filler(buf, "floating", NULL, 0, 0);
        filler(buf, "focused", NULL, 0, 0);
        return 0;
      }
    }
  }

  /* /focused */
  if (p.n == 1 && strcmp(p.parts[0], "focused") == 0) {
    Client *c = SELCLI();
    filler(buf, "name", NULL, 0, 0);
    filler(buf, "class", NULL, 0, 0);
    filler(buf, "geometry", NULL, 0, 0);
    filler(buf, "floating", NULL, 0, 0);
    filler(buf, "focused", NULL, 0, 0);
    return 0;
  }

  return -ENOENT;
}

/* ── read ───────────────────────────────────────────────────────────────── */

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  (void)fi;
  char out[512] = {0};

  Path p;
  path_parse(path, &p);

  /* /active_namespace */
  if (p.n == 1 && strcmp(p.parts[0], "active_namespace") == 0) {
    snprintf(out, sizeof out, "%s\n", wm.ns[wm.ans].name);
    goto done;
  }

  /* /focused/<field> */
  if (p.n == 2 && strcmp(p.parts[0], "focused") == 0) {
    Client *c = SELCLI();
    if (!c)
      return -ENOENT;
    const char *f = p.parts[1];
    if (strcmp(f, "name") == 0)
      snprintf(out, sizeof out, "%s\n", c->name);
    else if (strcmp(f, "class") == 0)
      snprintf(out, sizeof out, "%s\n", c->class);
    else if (strcmp(f, "geometry") == 0)
      snprintf(out, sizeof out, "%d %d %d %d\n", c->x, c->y, c->w, c->h);
    else if (strcmp(f, "floating") == 0)
      snprintf(out, sizeof out, "%d\n", c->isfloating);
    else if (strcmp(f, "focused") == 0)
      snprintf(out, sizeof out, "1\n");
    else
      return -ENOENT;
    goto done;
  }

  if (p.n >= 2 && strcmp(p.parts[0], "namespaces") == 0) {
    int ni = ns_index_by_name(p.parts[1]);
    if (ni < 0)
      return -ENOENT;
    Namespace *ns = &wm.ns[ni];

    /* /namespaces/<ns>/active */
    if (p.n == 3 && strcmp(p.parts[2], "active") == 0) {
      snprintf(out, sizeof out, "%u\n", ns->tagset[ns->seltags]);
      goto done;
    }

    if (p.n >= 4 && strcmp(p.parts[2], "tags") == 0) {
      int tagn = atoi(p.parts[3]);
      if (tagn < 1 || tagn > NTAGS)
        return -ENOENT;

      /* /namespaces/<ns>/tags/<n>/layout */
      if (p.n == 5 && strcmp(p.parts[4], "layout") == 0) {
        Tag *t = &ns->tags[tagn - 1];
        snprintf(out, sizeof out, "%s\n", t->lt[t->sellt]->symbol);
        goto done;
      }

      /* /namespaces/<ns>/tags/<n>/clients/<cdir>/<field> */
      if (p.n == 7 && strcmp(p.parts[4], "clients") == 0) {
        Client *c = client_by_dirname(p.parts[5]);
        if (!c)
          return -ENOENT;
        const char *f = p.parts[6];
        if (strcmp(f, "name") == 0)
          snprintf(out, sizeof out, "%s\n", c->name);
        else if (strcmp(f, "class") == 0)
          snprintf(out, sizeof out, "%s\n", c->class);
        else if (strcmp(f, "geometry") == 0)
          snprintf(out, sizeof out, "%d %d %d %d\n", c->x, c->y, c->w, c->h);
        else if (strcmp(f, "floating") == 0)
          snprintf(out, sizeof out, "%d\n", c->isfloating);
        else if (strcmp(f, "focused") == 0)
          snprintf(out, sizeof out, "%d\n", SELCLI() == c ? 1 : 0);
        else
          return -ENOENT;
        goto done;
      }
    }
  }

  return -ENOENT;

done: {
  size_t len = strlen(out);
  if (offset >= (off_t)len)
    return 0;
  if (offset + size > len)
    size = len - offset;
  memcpy(buf, out + offset, size);
  return (int)size;
}
}

/* ── write ──────────────────────────────────────────────────────────────── */

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  (void)offset;
  (void)fi;

  char val[256] = {0};
  size_t copy = size < sizeof val - 1 ? size : sizeof val - 1;
  memcpy(val, buf, copy);
  char *nl = strchr(val, '\n');
  if (nl)
    *nl = '\0';

  Path p;
  path_parse(path, &p);

  /* /active_namespace */
  if (p.n == 1 && strcmp(p.parts[0], "active_namespace") == 0) {
    int ni = ns_index_by_name(val);
    if (ni < 0)
      return -EINVAL;
    state_switchns(wm.selmon, ni);
    x11_arrange(wm.selmon);
    bar_draw(wm.selmon);
    return (int)size;
  }

  if (p.n >= 2 && strcmp(p.parts[0], "namespaces") == 0) {
    int ni = ns_index_by_name(p.parts[1]);
    if (ni < 0)
      return -ENOENT;

    /* /namespaces/<ns>/active */
    if (p.n == 3 && strcmp(p.parts[2], "active") == 0) {
      unsigned int tagmask = (unsigned int)strtoul(val, NULL, 10);
      Arg arg = {.ui = tagmask};
      state_seltag(&arg);
      return (int)size;
    }

    if (p.n >= 4 && strcmp(p.parts[2], "tags") == 0) {
      int tagn = atoi(p.parts[3]);
      if (tagn < 1 || tagn > NTAGS)
        return -ENOENT;

      /* client fields */
      if (p.n == 7 && strcmp(p.parts[4], "clients") == 0) {
        Client *c = client_by_dirname(p.parts[5]);
        if (!c)
          return -ENOENT;
        const char *f = p.parts[6];

        if (strcmp(f, "focused") == 0 && val[0] == '1') {
          x11_focus(c);
          return (int)size;
        }
        if (strcmp(f, "geometry") == 0 && c->isfloating) {
          int x, y, w, h;
          if (sscanf(val, "%d %d %d %d", &x, &y, &w, &h) == 4) {
            c->x = x;
            c->y = y;
            c->w = w;
            c->h = h;
            x11_arrange(wm.selmon);
          }
          return (int)size;
        }
        if (strcmp(f, "floating") == 0) {
          c->isfloating = (val[0] == '1');
          x11_arrange(wm.selmon);
          return (int)size;
        }
        if (strcmp(f, "tags") == 0) {
          unsigned int tags = (unsigned int)strtoul(val, NULL, 10);
          if (tags & TAGMASK) {
            c->tags = tags & TAGMASK;
            x11_arrange(wm.selmon);
          }
          return (int)size;
        }
      }

      /* layout */
      if (p.n == 5 && strcmp(p.parts[4], "layout") == 0) {
        /* find layout by symbol */
        unsigned int i;
        for (i = 0; i < LENGTH(layouts); i++) {
          if (strcmp(layouts[i].symbol, val) == 0) {
            Namespace *ns = &wm.ns[ni];
            Tag *t = &ns->tags[tagn - 1];
            t->lt[t->sellt] = &layouts[i];
            x11_arrange(wm.selmon);
            return (int)size;
          }
        }
        return -EINVAL;
      }
    }
  }

  return -EINVAL;
}

/* ── fuse ops table ─────────────────────────────────────────────────────── */

static const struct fuse_operations fs_ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .write = fs_write,
};

/* ── thread ─────────────────────────────────────────────────────────────── */

static void *fs_thread_main(void *arg) {
  (void)arg;
  char *argv[] = {"muhhwm", MOUNTPOINT, "-f", "-s", NULL};
  fuse_main(4, argv, &fs_ops, NULL);
  return NULL;
}

void fs_init(void) {
  if (mkdir(MOUNTPOINT, 0755) < 0 && errno != EEXIST) {
    fprintf(stderr, "muhhwm: fs_init: mkdir %s failed: %s\n", MOUNTPOINT,
            strerror(errno));
    return;
  }
  pthread_create(&fs_thread, NULL, fs_thread_main, NULL);
}

void fs_stop(void) {
  system("fusermount3 -u " MOUNTPOINT " 2>/dev/null");
  pthread_join(fs_thread, NULL);
}
