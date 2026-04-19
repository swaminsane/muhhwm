/* muhhtime.c - muhhwm activity dashboard
 * usage: muhhtime [today|week|month|DATE]
 * compile: cc -o muhhtime tools/muhhtime.c
 */

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACTIVITY_DIR "/.local/share/muhhwm/activity"
#define MAX_ENTRIES 100000
#define BAR_WIDTH 24

/* ── data ───────────────────────────────────────────────────────────────── */

typedef struct {
  long long ts;     /* unix timestamp */
  long long dur_ms; /* duration in ms */
  char ns[32];      /* namespace */
  int tag;          /* tag number */
  char cls[64];     /* window class */
  char title[256];  /* window title */
} Entry;

static Entry entries[MAX_ENTRIES];
static int nentries = 0;

/* ── aggregation ─────────────────────────────────────────────────────────── */

typedef struct {
  char key[256];
  long long ms;
} KV;
static KV ns_time[8];
static KV win_time[64];
static KV file_time[256];
static KV site_time[256];
static KV media_time[256];
static int n_ns, n_win, n_file, n_site, n_media;
static KV tab_time[512];
static int n_tab;

static void kv_add(KV *arr, int *n, int max, const char *key, long long ms) {
  int i;
  for (i = 0; i < *n; i++) {
    if (strcmp(arr[i].key, key) == 0) {
      arr[i].ms += ms;
      return;
    }
  }
  if (*n >= max)
    return;
  strncpy(arr[*n].key, key, 255);
  arr[*n].ms = ms;
  (*n)++;
}

static void kv_sort(KV *arr, int n) {
  int i, j;
  KV tmp;
  for (i = 0; i < n - 1; i++)
    for (j = i + 1; j < n; j++)
      if (arr[j].ms > arr[i].ms) {
        tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
      }
}

/* ── parsing ─────────────────────────────────────────────────────────────── */

static int load_log_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  char line[512];
  while (fgets(line, sizeof line, f) && nentries < MAX_ENTRIES) {
    Entry *e = &entries[nentries];
    char title_raw[256] = {0};
    if (sscanf(line, "%lld %lld %31s %d %63s \"%255[^\"]\"", &e->ts, &e->dur_ms,
               e->ns, &e->tag, e->cls, title_raw) == 6) {
      strncpy(e->title, title_raw, sizeof e->title - 1);
      if (e->dur_ms > 0)
        nentries++;
    }
  }
  fclose(f);
  return nentries;
}

static int load_log(void) {
  char dir[512];
  const char *home = getenv("HOME");
  if (!home)
    return 0;
  snprintf(dir, sizeof dir, "%s%s", home, ACTIVITY_DIR);

  DIR *d = opendir(dir);
  if (!d) {
    fprintf(stderr, "muhhtime: no activity dir at %s\n", dir);
    return 0;
  }

  struct dirent *ent;
  char path[600];
  while ((ent = readdir(d))) {
    if (strstr(ent->d_name, ".log")) {
      snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
      load_log_file(path);
    }
  }
  closedir(d);
  return nentries;
}

/* extract domain from firefox title — "Page Title - site.com" → "site.com" */
static void extract_domain(const char *title, char *out, size_t sz) {
  const char *p = strrchr(title, '-');
  if (!p) {
    strncpy(out, title, sz - 1);
    return;
  }
  p++;
  while (*p == ' ')
    p++;
  strncpy(out, p, sz - 1);
  /* strip trailing whitespace */
  size_t len = strlen(out);
  while (len > 0 && isspace((unsigned char)out[len - 1]))
    out[--len] = '\0';
}

/* extract filename from nvim title — "NVIM /path/to/file" → "file" */
static void extract_nvim_file(const char *title, char *out, size_t sz) {
  const char *p = strrchr(title, '/');
  if (p)
    p++;
  else
    p = title;
  /* skip "NVIM " prefix if present */
  if (strncmp(p, "NVIM ", 5) == 0)
    p += 5;
  strncpy(out, p, sz - 1);
}

/* extract filename from st title — "st: /path/to/dir" → "/path/to/dir" */
static void extract_st_dir(const char *title, char *out, size_t sz) {
  const char *p = title;
  if (strncmp(p, "st: ", 4) == 0)
    p += 4;
  strncpy(out, p, sz - 1);
}

static void aggregate(long long from, long long to) {
  int i;
  char key[256];

  n_ns = n_win = n_file = n_site = n_media = 0;

  for (i = 0; i < nentries; i++) {
    Entry *e = &entries[i];
    if (e->ts < from || e->ts >= to)
      continue;

    /* namespace time */
    kv_add(ns_time, &n_ns, 8, e->ns, e->dur_ms);

    /* window class time */
    kv_add(win_time, &n_win, 64, e->cls, e->dur_ms);

    /* nvim files */
    if (strstr(e->title, "NVIM") || strstr(e->title, "nvim")) {
      extract_nvim_file(e->title, key, sizeof key);
      if (key[0])
        kv_add(file_time, &n_file, 256, key, e->dur_ms);
    }

    /* st directory */
    if (strcmp(e->cls, "St") == 0 && strncmp(e->title, "st: ", 4) == 0) {
      extract_st_dir(e->title, key, sizeof key);
      if (key[0])
        kv_add(file_time, &n_file, 256, key, e->dur_ms);
    }

    /* firefox/surf sites */
    if (strcmp(e->cls, "firefox") == 0 || strcmp(e->cls, "Firefox") == 0 ||
        strcmp(e->cls, "firefox-esr") == 0 || strcmp(e->cls, "Surf") == 0 ||
        strcmp(e->cls, "tabbed") == 0) {
      /* skip useless titles */
      if (strncmp(e->title, "tabbed-", 7) == 0)
        continue;
      if (strncmp(e->title, "st: ", 4) == 0)
        continue;
      if (strcmp(e->title, "Mozilla Firefox") == 0)
        continue;
      if (strstr(e->title, "%]"))
        continue;
      if (strcmp(e->title, "broken") == 0)
        continue;
      /* strip surf loading indicator [X%] prefix */
      const char *t = e->title;
      if (t[0] == '[') {
        const char *p = strchr(t, ']');
        if (p)
          t = p + 2;
      }
      /* strip surf keybind prefix "X | " */
      if (strstr(t, " | ")) {
        const char *p = strstr(t, " | ");
        t = p + 3;
      }
      /* strip Mozilla Firefox suffix */
      char clean[256];
      strncpy(clean, t, sizeof clean - 1);
      clean[sizeof clean - 1] = '\0';
      char *moz = strstr(clean, "  Mozilla Firefox");
      if (!moz)
        moz = strstr(clean, " - Mozilla Firefox");
      if (moz)
        *moz = '\0';
      t = clean;

      /* domain for default view */
      extract_domain(t, key, sizeof key);
      if (key[0])
        kv_add(site_time, &n_site, 256, key, e->dur_ms);
      /* full title for tabs view */
      if (t[0]) {
        char labeled[280];
        const char *browser =
            (strcmp(e->cls, "Surf") == 0 || strcmp(e->cls, "tabbed") == 0)
                ? "[Surf] "
                : "[Firefox] ";
        snprintf(labeled, sizeof labeled, "%s%s", browser, t);
        kv_add(tab_time, &n_tab, 512, labeled, e->dur_ms);
      }
    }

  } /* end for loop */
} /* end aggregate */

/* ── rendering
 * ───────────────────────────────────────────────────────────── */

static void fmt_duration(long long ms, char *out, size_t sz) {
  long long s = ms / 1000;
  long long m = s / 60;
  long long h = m / 60;
  m %= 60;
  if (h > 0)
    snprintf(out, sz, "%lldh %02lldm", h, m);
  else if (m > 0)
    snprintf(out, sz, "%lldm %02llds", m, s % 60);
  else
    snprintf(out, sz, "%llds", s);
}

static void print_bar(long long ms, long long total_ms) {
  int filled = (total_ms > 0) ? (int)((double)ms / total_ms * BAR_WIDTH) : 0;
  int i;
  printf("  ");
  for (i = 0; i < BAR_WIDTH; i++)
    printf("%s", i < filled ? "█" : "░");
}

static void print_separator(void) {
  int i;
  for (i = 0; i < 56; i++)
    printf("─");
  printf("\n");
}

static void print_kv_section(const char *title, KV *arr, int n, int max_rows) {
  if (n == 0)
    return;
  char dur[32];
  int i;
  long long total = arr[0].ms;

  printf("\n%s\n", title);
  for (i = 0; i < n && i < max_rows; i++) {
    fmt_duration(arr[i].ms, dur, sizeof dur);
    print_bar(arr[i].ms, total);
    printf("  %-32s  %s\n", arr[i].key, dur);
  }
}

static void print_hourly(long long from, long long to) {
  long long hour_ms[24] = {0};
  long long max_ms = 1;
  int i, h;

  for (i = 0; i < nentries; i++) {
    Entry *e = &entries[i];
    if (e->ts < from || e->ts >= to)
      continue;
    struct tm *t = localtime((time_t *)&e->ts);
    h = t->tm_hour;
    hour_ms[h] += e->dur_ms;
    if (hour_ms[h] > max_ms)
      max_ms = hour_ms[h];
  }

  printf("\nHOURLY ACTIVITY\n");
  for (h = 0; h < 24; h++) {
    if (hour_ms[h] == 0)
      continue;
    int filled = (int)((double)hour_ms[h] / max_ms * BAR_WIDTH);
    char dur[32];
    fmt_duration(hour_ms[h], dur, sizeof dur);
    printf("  %02d  ", h);
    for (i = 0; i < filled; i++)
      printf("▓");
    printf("  %s\n", dur);
  }
}

static void print_streaks(long long from, long long to) {
  long long longest = 0, total_dur = 0, switches = 0;
  long long streak_start = 0, streak_dur = 0;
  char streak_ns[32] = {0};
  int i;

  for (i = 0; i < nentries; i++) {
    Entry *e = &entries[i];
    if (e->ts < from || e->ts >= to)
      continue;
    total_dur += e->dur_ms;
    switches++;
    if (strcmp(e->ns, streak_ns) == 0) {
      streak_dur += e->dur_ms;
    } else {
      if (streak_dur > longest) {
        longest = streak_dur;
      }
      strncpy(streak_ns, e->ns, sizeof streak_ns - 1);
      streak_start = e->ts;
      streak_dur = e->dur_ms;
    }
  }
  if (streak_dur > longest)
    longest = streak_dur;

  char dur[32], total[32];
  fmt_duration(longest, dur, sizeof dur);
  fmt_duration(total_dur, total, sizeof total);

  printf("\nFOCUS\n");
  printf("  total active     %s\n", total);
  printf("  longest streak   %s\n", dur);
  printf("  focus switches   %lld\n", switches);
  if (switches > 0)
    printf("  avg per window   ");
  char avg[32];
  fmt_duration(switches > 0 ? total_dur / switches : 0, avg, sizeof avg);
  printf("%s\n", avg);
  (void)streak_start;
}

/* ── date helpers
 * ───────────────────────────────────────────────────────────
 */

static long long day_start(time_t t) {
  struct tm *tm = localtime(&t);
  tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
  return (long long)mktime(tm);
}

static long long week_start(time_t t) {
  struct tm *tm = localtime(&t);
  tm->tm_mday -= tm->tm_wday;
  tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
  return (long long)mktime(tm);
}

/* ── main
 * ───────────────────────────────────────────────────────────────────
 */

int main(int argc, char *argv[]) {
  time_t now = time(NULL);
  long long from, to;
  char label[64] = "today";

  if (!load_log())
    return 1;

  /* parse range argument */
  if (argc < 2 || strcmp(argv[1], "today") == 0) {
    from = day_start(now);
    to = from + 86400;
    struct tm *tm = localtime(&now);
    char date[32];
    strftime(date, sizeof date, "%b %d %Y, %A", tm);
    snprintf(label, sizeof label, "%s", date);
  } else if (strcmp(argv[1], "week") == 0) {
    from = week_start(now);
    to = from + 7 * 86400;
    snprintf(label, sizeof label, "this week");
  } else if (strcmp(argv[1], "month") == 0) {
    struct tm *tm = localtime(&now);
    tm->tm_mday = 1;
    tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
    from = (long long)mktime(tm);
    to = from + 31 * 86400LL;
    snprintf(label, sizeof label, "this month");
  } else if (strcmp(argv[1], "tabs") == 0) {
    from = day_start(now);
    to = from + 86400;
    aggregate(from, to);
    printf("\n");
    print_separator();
    printf("  TABS TODAY\n");
    print_separator();
    print_kv_section("FIREFOX / SURF", tab_time, n_tab, 50);
    printf("\n");
    print_separator();
    printf("\n");
    return 0;
  } else if (strcmp(argv[1], "recent") == 0) {
    int count = argc > 2 ? atoi(argv[2]) : 10;
    printf("\n  RECENT ACTIVITY\n");
    print_separator();
    int start = nentries - count;
    if (start < 0)
      start = 0;
    int i;
    for (i = nentries - 1; i >= start; i--) {
      Entry *e = &entries[i];
      struct tm *t = localtime((time_t *)&e->ts);
      char dur[16], timestr[8];
      strftime(timestr, sizeof timestr, "%H:%M", t);
      fmt_duration(e->dur_ms, dur, sizeof dur);
      printf("  %s  %-12s  %-36s  %s\n", timestr, e->cls, e->title, dur);
    }
    printf("\n");
    return 0;
  } else {
    /* try parsing as YYYY-MM-DD */
    struct tm tm = {0};
    if (strptime(argv[1], "%Y-%m-%d", &tm)) {
      from = (long long)mktime(&tm);
      to = from + 86400;
      snprintf(label, sizeof label, "%s", argv[1]);
    } else {
      fprintf(stderr, "usage: muhhtime [today|week|month|YYYY-MM-DD]\n");
      return 1;
    }
  }

  aggregate(from, to);

  /* header */
  printf("\n");
  print_separator();
  printf("  muhhtime — %s\n", label);
  print_separator();

  /* namespace breakdown */
  print_kv_section("NAMESPACES", ns_time, n_ns, 8);

  /* hourly */
  print_hourly(from, to);

  /* streaks */
  print_streaks(from, to);

  /* top windows */
  print_kv_section("TOP WINDOWS", win_time, n_win, 6);

  /* nvim files / st dirs */
  print_kv_section("NVIM / TERMINAL", file_time, n_file, 10);

  /* firefox */
  print_kv_section("FIREFOX", site_time, n_site, 10);

  /* mpv */
  print_kv_section("MPV", media_time, n_media, 10);

  printf("\n");
  print_separator();
  printf("\n");

  return 0;
}
