#define _POSIX_C_SOURCE 200809L
#include "common.h"

#define YT_PID "/tmp/yt_mpv.pid"
#define RAD_PID "/tmp/radio.pid"
#define POD_PID "/tmp/podcast.pid"
#define YT_SOCK "/tmp/yt_mpv.sock"
#define RAD_SOCK "/tmp/radio_mpv.sock"
#define POD_SOCK "/tmp/podcast.sock"

static char media_text[16] = "[OFF]";

static int pid_live(const char *f) {
  FILE *p = fopen(f, "r");
  if (!p)
    return 0;
  int pid = 0;
  (void)fscanf(p, "%d", &pid);
  fclose(p);
  if (pid <= 0)
    return 0;
  char path[32];
  snprintf(path, sizeof path, "/proc/%d", pid);
  p = fopen(path, "r");
  if (p) {
    fclose(p);
    return 1;
  }
  return 0;
}

static int mpv_paused(const char *sock) {
  char cmd[128];
  snprintf(cmd, sizeof cmd,
           "echo '{\"command\":[\"get_property\",\"pause\"]}' | "
           "socat - %s 2>/dev/null | grep -q '\"data\":true'",
           sock);
  return system(cmd) == 0;
}

void media_update(void) {
  if (pid_live(YT_PID)) {
    snprintf(media_text, sizeof media_text,
             mpv_paused(YT_SOCK) ? "[|| YT]" : "[> YT]");
  } else if (pid_live(RAD_PID)) {
    snprintf(media_text, sizeof media_text, "[> RAD]");
  } else if (pid_live(POD_PID)) {
    snprintf(media_text, sizeof media_text,
             mpv_paused(POD_SOCK) ? "[|| POD]" : "[> POD]");
  } else {
    FILE *f = popen("/usr/bin/mpc status 2>/dev/null", "r");
    media_text[0] = '\0';
    if (f) {
      char line[64];
      while (fgets(line, sizeof line, f)) {
        if (strstr(line, "[playing]")) {
          snprintf(media_text, sizeof media_text, "[> MPC]");
          break;
        } else if (strstr(line, "[paused]")) {
          snprintf(media_text, sizeof media_text, "[|| MPC]");
          break;
        }
      }
      pclose(f);
    }
    if (!media_text[0])
      snprintf(media_text, sizeof media_text, "[OFF]");
  }
}

int media_draw(int x) {
  int sch = (media_text[1] == 'O' || media_text[1] == '|') ? SchGrey : SchNorm;
  drw_setscheme(drw, scheme[sch]);
  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_MEDIA].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), media_text, 0);
}

void media_click(int button) {
  if (button == 1) {
    if (pid_live(YT_PID)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/yt_mpv.sock",
                                  NULL};
      spawn(cmd);
    } else if (pid_live(RAD_PID)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/radio_mpv.sock",
                                  NULL};
      spawn(cmd);
    } else if (pid_live(POD_PID)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/podcast.sock",
                                  NULL};
      spawn(cmd);
    } else {
      static const char *cmd[] = {"/usr/bin/mpc", "toggle", NULL};
      spawn(cmd);
    }
    focusmods[MOD_MEDIA].updated = 0;
  } else if (button == 2) {
    /* middle click: toggle MPD stop/start */
    FILE *f = popen("/usr/bin/mpc status 2>/dev/null", "r");
    int stopped = 1;
    if (f) {
      char line[64];
      while (fgets(line, sizeof line, f))
        if (strstr(line, "[playing]") || strstr(line, "[paused]")) {
          stopped = 0;
          break;
        }
      pclose(f);
    }
    if (stopped) {
      static const char *cmd[] = {"/usr/bin/mpc", "play", NULL};
      spawn(cmd);
    } else {
      static const char *cmd[] = {"/usr/bin/mpc", "stop", NULL};
      spawn(cmd);
    }
    focusmods[MOD_MEDIA].updated = 0;
  } else if (button == 3) {
    static const char *cmd[] = {"/bin/bash", "-c",
                                "$HOME/.local/bin/menu/music/musicmenu", NULL};
    spawn(cmd);
  }
}

void media_scroll(int dir) { (void)dir; }
