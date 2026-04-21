/* activity.c - muhhbar activity module
 * reads today's log files, filters by timestamp, matches muhhtime output
 */
#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <dirent.h>

#define NNAMESPACES 3
#define MAX_ENTRIES 100000

static const char *nsnames[NNAMESPACES] = {"study", "code", "free"};
static const int nsschemes[NNAMESPACES] = {SchNsStudy, SchNsCode, SchNsFree};

/* aggregated totals */
static long long ns_ms[NNAMESPACES];
static long long total_ms = 0;

/* ── helpers ─────────────────────────────────────────────────────────── */

static long long day_start_ts(void) {
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
  return (long long)mktime(tm);
}

/* ── update ──────────────────────────────────────────────────────────── */

void activity_update(void) {
  const char *home = getenv("HOME");
  if (!home)
    return;

  long long from = day_start_ts();
  long long to = from + 86400LL;

  int i;
  for (i = 0; i < NNAMESPACES; i++)
    ns_ms[i] = 0;
  total_ms = 0;

  /* build activity dir path */
  char dir[512];
  snprintf(dir, sizeof dir, "%s/.local/share/muhhwm/activity", home);

  DIR *d = opendir(dir);
  if (!d)
    return;

  struct dirent *ent;
  while ((ent = readdir(d))) {
    /* only process .log files */
    if (!strstr(ent->d_name, ".log"))
      continue;

    char path[768];
    snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);

    FILE *f = fopen(path, "r");
    if (!f)
      continue;

    char line[512];
    int count = 0;
    while (fgets(line, sizeof line, f) && count < MAX_ENTRIES) {
      long long ts, dur;
      char ns[32], cls[64];
      char title[256] = {0};
      int tag;

      /* match muhhtime's exact sscanf pattern */
      if (sscanf(line, "%lld %lld %31s %d %63s \"%255[^\"]\"", &ts, &dur, ns,
                 &tag, cls, title) < 5)
        continue;

      if (dur <= 0)
        continue;

      /* only count entries within today's window */
      if (ts < from || ts >= to)
        continue;

      /* add to namespace totals */
      for (i = 0; i < NNAMESPACES; i++) {
        if (strcmp(ns, nsnames[i]) == 0) {
          ns_ms[i] += dur;
          break;
        }
      }
      total_ms += dur;
      count++;
    }
    fclose(f);
  }
  closedir(d);
}

/* ── format ──────────────────────────────────────────────────────────── */

static void fmt_ms(long long ms, char *buf, size_t sz) {
  int secs = (int)(ms / 1000);
  int hrs = secs / 3600;
  int mins = (secs % 3600) / 60;
  if (hrs > 0)
    snprintf(buf, sz, "%dh%02dm", hrs, mins);
  else
    snprintf(buf, sz, "%dm", mins);
}

/* ── draw ────────────────────────────────────────────────────────────── */

int activity_draw(int x) {
  int i;

  if (detail_view) {
    /* full bar: per-namespace breakdown */
    int slot = barw / NNAMESPACES;
    int cx = 0;
    for (i = 0; i < NNAMESPACES; i++) {
      char dur[12];
      char text[32];
      fmt_ms(ns_ms[i], dur, sizeof dur);
      snprintf(text, sizeof text, "%s %s", nsnames[i], dur);
      drw_setscheme(drw, scheme[nsschemes[i]]);
      drw_text(drw, cx, 0, (unsigned int)slot, (unsigned int)barh,
               (unsigned int)(lrpad / 2), text, 0);
      cx += slot;
    }
    return barw;
  }

  /* normal: total screen time */
  char dur[12], text[18];
  fmt_ms(total_ms, dur, sizeof dur);
  snprintf(text, sizeof text, "S %s", dur);

  drw_setscheme(drw, scheme[SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_ACTIVITY].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

/* ── click / scroll ──────────────────────────────────────────────────── */

void activity_click(int button) {
  if (button == 1) {
    activity_update();
    focusmods[MOD_ACTIVITY].updated = time(NULL);
    detail_view = 1;
    detail_time = time(NULL);
  }
}

void activity_scroll(int dir) { (void)dir; }
