#define _POSIX_C_SOURCE 200809L
#include "common.h"

static char texts_title[48] = "[No book]";
static char texts_path[512] = "";

void texts_update(void) {
  const char *home = getenv("HOME");
  const char *cache = getenv("XDG_CACHE_HOME");
  char recent[256];

  if (cache && cache[0])
    snprintf(recent, sizeof recent, "%s/textsmenu_recent", cache);
  else
    snprintf(recent, sizeof recent, "%s/.cache/textsmenu_recent", home);

  FILE *f = fopen(recent, "r");
  if (!f) {
    snprintf(texts_title, sizeof texts_title, "[No book]");
    texts_path[0] = '\0';
    return;
  }

  texts_path[0] = '\0';
  (void)fgets(texts_path, sizeof texts_path, f);
  fclose(f);

  char *nl = strchr(texts_path, '\n');
  if (nl)
    *nl = '\0';

  if (!texts_path[0]) {
    snprintf(texts_title, sizeof texts_title, "[No book]");
    return;
  }

  /* extract filename without extension */
  const char *base = strrchr(texts_path, '/');
  base = base ? base + 1 : texts_path;

  char name[48];
  snprintf(name, sizeof name, "%s", base);

  /* strip extension */
  char *dot = strrchr(name, '.');
  if (dot)
    *dot = '\0';

  snprintf(texts_title, sizeof texts_title, "[%.18s]", name);
}

static const char *get_reader(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "zathura";
  ext++;
  if (strcmp(ext, "epub") == 0 || strcmp(ext, "mobi") == 0)
    return "mupdf";
  return "zathura";
}

int texts_draw(int x) {
  int sch = texts_path[0] ? SchNorm : SchGrey;
  drw_setscheme(drw, scheme[sch]);
  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_TEXTS].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), texts_title,
                  0);
}

void texts_click(int button) {
  if (button == 1 && texts_path[0]) {
    /* open last read book in its reader */
    const char *reader = get_reader(texts_path);
    char cmd[600];
    snprintf(cmd, sizeof cmd, "%s \"%s\" &", reader, texts_path);
    const char *argv[] = {"/bin/sh", "-c", cmd, NULL};
    spawn(argv);
  } else if (button == 2) {
    /* middle click — Books library */
    const char *argv[] = {"/bin/bash", "-c",
                          "$HOME/.local/bin/menu/textsmenu Books", NULL};
    spawn(argv);
  } else if (button == 3) {
    /* right click — NCERT library */
    const char *argv[] = {"/bin/bash", "-c",
                          "$HOME/.local/bin/menu/textsmenu NCERT", NULL};
    spawn(argv);
  }
}

void texts_scroll(int dir) { (void)dir; }
