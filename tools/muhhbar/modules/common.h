/* common.h - shared helpers for all module files */
#ifndef MOD_COMMON_H
#define MOD_COMMON_H

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"
#include "../drw.h"
#include "../modules.h"
#include "../muhhbar.h"
#include "../util.h"

static inline int read_int_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  int v = 0;
  (void)fscanf(f, "%d", &v);
  fclose(f);
  return v;
}

static inline int popen_int(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return 0;
  int v = 0;
  (void)fscanf(f, "%d", &v);
  pclose(f);
  return v;
}

#endif /* MOD_COMMON_H */
