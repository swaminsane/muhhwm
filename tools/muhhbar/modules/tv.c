#define _POSIX_C_SOURCE 200809L
#include "common.h"

#define TV_STATE "/tmp/tvapp.state"

static char tv_text[64] = "";

void tv_update(void) {
  FILE *f = fopen(TV_STATE, "r");
  if (!f) {
    snprintf(tv_text, sizeof tv_text, "[TV Off]");
    return;
  }
  char ch[48] = {0};
  (void)fgets(ch, sizeof ch, f);
  fclose(f);
  char *nl = strchr(ch, '\n');
  if (nl)
    *nl = '\0';
  if (ch[0])
    snprintf(tv_text, sizeof tv_text, "[%.40s]", ch);
  else
    snprintf(tv_text, sizeof tv_text, "[TV Off]");
}

int tv_draw(int x) {
  drw_setscheme(drw, scheme[tv_text[1] == 'T' ? SchGrey : SchNorm]);
  return drw_text(drw, x, 0, (unsigned int)mediamods[MOD_TV].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), tv_text, 0);
}

void tv_click(int button) {
  if (button == 1) {
    static const char *cmd[] = {"/bin/bash", "-c",
                                "$HOME/.local/bin/menu/tvmenu", NULL};
    spawn(cmd);
  }
}

void tv_scroll(int dir) { (void)dir; }
