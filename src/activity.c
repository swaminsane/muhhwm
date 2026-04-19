/* activity.c - muhhwm activity logger
 * logs focus events to ~/.local/share/muhhwm/activity.log
 * format: TIMESTAMP DURATION_MS NAMESPACE TAG CLASS "TITLE"
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "muhh.h"

#define ACTIVITY_DIR "/.local/share/muhhwm/activity"

/* current focus state */
static struct {
  char ns[NNAMELEN];
  int tag;
  char class[64];
  char title[256];
  int active;
  struct timespec wall_start;
  struct timespec mono_start;
} cur;

static char logpath[512];

static long long ms_since(struct timespec *start) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (long long)(now.tv_sec - start->tv_sec) * 1000 +
         (long long)(now.tv_nsec - start->tv_nsec) / 1000000;
}

static void ensure_dir(void) {
  char dir[512];
  char parent[512];
  const char *home = getenv("HOME");
  if (!home)
    return;

  /* create parent dir */
  snprintf(parent, sizeof parent, "%s/.local/share/muhhwm", home);
  mkdir(parent, 0755);

  /* create activity subdir */
  snprintf(dir, sizeof dir, "%s%s", home, ACTIVITY_DIR);
  mkdir(dir, 0755);

  /* today's dated log file */
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char date[16];
  strftime(date, sizeof date, "%Y-%m-%d", tm);
  snprintf(logpath, sizeof logpath, "%s/%s.log", dir, date);
}

void activity_init(void) {
  ensure_dir();
  memset(&cur, 0, sizeof cur);
}

void activity_focus(Client *c) {
  FILE *f;
  long long duration;

  if (cur.active) {
    duration = ms_since(&cur.mono_start);
    f = fopen(logpath, "a");
    if (f) {
      fprintf(f, "%lld %lld %s %d %s \"%s\"\n",
              (long long)cur.wall_start.tv_sec, duration,
              cur.ns[0] ? cur.ns : "none", cur.tag,
              cur.class[0] ? cur.class : "unknown", cur.title);
      fclose(f);
    }
  }

  if (!c) {
    cur.active = 0;
    return;
  }

  clock_gettime(CLOCK_REALTIME, &cur.wall_start);
  clock_gettime(CLOCK_MONOTONIC, &cur.mono_start);

  Namespace *ns = &wm.ns[c->ns];
  strncpy(cur.ns, ns->name, sizeof cur.ns - 1);
  strncpy(cur.class, c->class, sizeof cur.class - 1);
  strncpy(cur.title, c->name, sizeof cur.title - 1);
  cur.tag = __builtin_ctz(ns->tagset[ns->seltags]) + 1;
  cur.active = 1;
}

void activity_update_title(Client *c) {
  if (!cur.active || !c)
    return;
  /* only update if this client matches current tracked entry */
  Namespace *ns = &wm.ns[c->ns];
  if (strcmp(cur.ns, ns->name) != 0)
    return;
  if (cur.tag != __builtin_ctz(ns->tagset[ns->seltags]) + 1)
    return;
  strncpy(cur.title, c->name, sizeof cur.title - 1);
}

void activity_flush(void) {
  if (cur.active)
    activity_focus(NULL);
}
