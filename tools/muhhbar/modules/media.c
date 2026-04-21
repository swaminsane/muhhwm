/* media.c - muhhbar media module
 * shows current track, left click toggles play/pause, right click opens
 * musicmenu
 */
#define _POSIX_C_SOURCE 200809L
#include "common.h"

#define YT_PID_FILE "/tmp/yt_mpv.pid"
#define RADIO_PID_FILE "/tmp/radio.pid"
#define PODCAST_PID_FILE "/tmp/podcast.pid"
#define YT_SOCKET "/tmp/yt_mpv.sock"
#define RADIO_SOCKET "/tmp/radio_mpv.sock"
#define PODCAST_SOCKET "/tmp/podcast.sock"

/* display state */
static char media_text[64] = "--";
static int media_paused = 0;

/* ── helpers ─────────────────────────────────────────────────────────── */

static int pid_running(const char *pidfile) {
  FILE *f = fopen(pidfile, "r");
  if (!f)
    return 0;
  int pid = 0;
  (void)fscanf(f, "%d", &pid);
  fclose(f);
  if (pid <= 0)
    return 0;
  /* kill -0 equivalent: check /proc/pid */
  char path[32];
  snprintf(path, sizeof path, "/proc/%d", pid);
  FILE *p = fopen(path, "r");
  if (p) {
    fclose(p);
    return 1;
  }
  return 0;
}

/* query mpv IPC socket for a string property */
static int mpv_get_str(const char *socket, const char *prop, char *out,
                       size_t sz) {
  char cmd[256];
  snprintf(cmd, sizeof cmd,
           "echo '{\"command\":[\"get_property\",\"%s\"]}' | "
           "socat - %s 2>/dev/null | "
           "python3 -c \""
           "import sys,json;"
           "d=json.load(sys.stdin);"
           "print(d.get('data','') or '')\" 2>/dev/null",
           prop, socket);
  FILE *f = popen(cmd, "r");
  if (!f)
    return 0;
  out[0] = '\0';
  (void)fgets(out, (int)sz, f);
  pclose(f);
  /* strip newline */
  char *nl = strchr(out, '\n');
  if (nl)
    *nl = '\0';
  return out[0] != '\0';
}

static int mpv_get_paused(const char *socket) {
  char buf[16];
  if (!mpv_get_str(socket, "pause", buf, sizeof buf))
    return 0;
  return strcmp(buf, "True") == 0;
}

/* truncate title to fit module width */
static void truncate_title(const char *prefix, const char *title, char *out,
                           size_t sz) {
  /* max chars that fit = (width - lrpad) / avg_char_width */
  int max_chars = (focusmods[MOD_MEDIA].width - lrpad) / 8;
  if (max_chars < 4)
    max_chars = 4;
  char tmp[256];
  snprintf(tmp, sizeof tmp, "%s %s", prefix, title);
  if ((int)strlen(tmp) > max_chars) {
    tmp[max_chars - 1] = '.';
    tmp[max_chars - 0] = '.';
    tmp[max_chars + 1] = '\0';
  }
  snprintf(out, sz, "%s", tmp);
}

/* ── update ──────────────────────────────────────────────────────────── */

void media_update(void) {
  char title[256] = {0};

  if (pid_running(YT_PID_FILE)) {
    mpv_get_str(YT_SOCKET, "media-title", title, sizeof title);
    media_paused = mpv_get_paused(YT_SOCKET);
    truncate_title("YT", title[0] ? title : "...", media_text,
                   sizeof media_text);

  } else if (pid_running(RADIO_PID_FILE)) {
    mpv_get_str(RADIO_SOCKET, "media-title", title, sizeof title);
    media_paused = 0; /* radio is always live */
    truncate_title("~", title[0] ? title : "Radio", media_text,
                   sizeof media_text);

  } else if (pid_running(PODCAST_PID_FILE)) {
    mpv_get_str(PODCAST_SOCKET, "media-title", title, sizeof title);
    media_paused = mpv_get_paused(PODCAST_SOCKET);
    truncate_title("Pod", title[0] ? title : "...", media_text,
                   sizeof media_text);

  } else {
    /* MPC */
    FILE *f = popen("/usr/bin/mpc current 2>/dev/null", "r");
    if (f) {
      (void)fgets(title, sizeof title, f);
      pclose(f);
      char *nl = strchr(title, '\n');
      if (nl)
        *nl = '\0';
    }

    if (title[0]) {
      /* check pause state */
      FILE *s = popen("/usr/bin/mpc status 2>/dev/null", "r");
      media_paused = 1; /* assume paused unless [playing] found */
      if (s) {
        char line[128];
        while (fgets(line, sizeof line, s)) {
          if (strstr(line, "[playing]")) {
            media_paused = 0;
            break;
          }
        }
        pclose(s);
      }
      truncate_title("", title, media_text, sizeof media_text);
    } else {
      snprintf(media_text, sizeof media_text, "--");
      media_paused = 0;
    }
  }
}

/* ── draw ────────────────────────────────────────────────────────────── */

int media_draw(int x) {
  int sch = SchNorm;
  if (strcmp(media_text, "--") == 0)
    sch = SchGrey;
  else if (media_paused)
    sch = SchGrey;

  /* prepend play/pause indicator */
  char display[72];
  if (strcmp(media_text, "--") == 0) {
    snprintf(display, sizeof display, "--");
  } else {
    snprintf(display, sizeof display, "%s %s", media_paused ? "||" : ">",
             media_text);
  }

  drw_setscheme(drw, scheme[sch]);
  return drw_text(drw, x, 0, (unsigned int)focusmods[MOD_MEDIA].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), display, 0);
}

/* ── click ───────────────────────────────────────────────────────────── */

void media_click(int button) {
  if (button == 1) {
    /* toggle play/pause */
    if (pid_running(YT_PID_FILE)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/yt_mpv.sock",
                                  NULL};
      spawn(cmd);
    } else if (pid_running(RADIO_PID_FILE)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/radio_mpv.sock",
                                  NULL};
      spawn(cmd);
    } else if (pid_running(PODCAST_PID_FILE)) {
      static const char *cmd[] = {"/bin/sh", "-c",
                                  "echo '{\"command\":[\"cycle\",\"pause\"]}' "
                                  "| socat - /tmp/podcast.sock",
                                  NULL};
      spawn(cmd);
    } else {
      static const char *cmd[] = {"/usr/bin/mpc", "toggle", NULL};
      spawn(cmd);
    }
    /* force immediate update */
    focusmods[MOD_MEDIA].updated = 0;
  } else if (button == 3) {
    static const char *cmd[] = {"/bin/sh", "-c",
                                "$HOME/.local/bin/menu/music/musicmenu", NULL};
    spawn(cmd);
  }
}

void media_scroll(int dir) { (void)dir; }
