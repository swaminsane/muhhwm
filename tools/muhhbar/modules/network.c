#define _POSIX_C_SOURCE 200809L
#include "common.h"

/* 0=off, 1=on but not connected, 2=connected */
static int wifi_state = 0;
static int bt_state = 0;

/* ---------------------------
 * COLOR MAPPING
 * --------------------------- */
static int wifi_sch_from_state(int s) {
  if (s == 1)
    return SchGreen; /* connected */
  return SchGrey;    /* off */
}

static int bt_sch_from_state(int s) {
  if (s == 2)
    return SchGreen;
  if (s == 1)
    return SchNorm;
  return SchGrey;
}

/* ---------------------------
 * UPDATE NETWORK STATE
 * --------------------------- */
void network_update(void) {
  FILE *f;
  char ssid[64] = {0};

  /* -------- WiFi -------- */
  f = popen("iwgetid -r 2>/dev/null", "r");
  if (f) {
    fgets(ssid, sizeof ssid, f);
    pclose(f);

    char *nl = strchr(ssid, '\n');
    if (nl)
      *nl = '\0';

    if (ssid[0] != '\0') {
      wifi_state = 2; /* connected */
    } else {
      /* interface up but no SSID */
      int up =
          popen_int("cat /sys/class/net/w*/operstate 2>/dev/null | grep -c up");
      wifi_state = (up > 0) ? 1 : 0;
    }
  }

  /* -------- Bluetooth -------- */
  int powered =
      popen_int("bluetoothctl show 2>/dev/null | grep -c 'Powered: yes'");

  if (!powered) {
    bt_state = 0;
  } else {
    int connected =
        popen_int("bluetoothctl devices Connected 2>/dev/null | wc -l");
    bt_state = (connected > 0) ? 2 : 1;
  }
}

/* ---------------------------
 * DRAW
 * --------------------------- */
int network_draw(int x) {
  int lp = lrpad / 2;

  int w1 = (int)drw_fontset_getwidth(drw, "W ");
  int w2 = (int)drw_fontset_getwidth(drw, "B");

  /* WiFi */
  drw_setscheme(drw, scheme[wifi_sch_from_state(wifi_state)]);
  drw_text(drw, x, 0, (unsigned int)(w1 + lp), (unsigned int)barh,
           (unsigned int)lp, "W ", 0);

  /* Bluetooth */
  drw_setscheme(drw, scheme[bt_sch_from_state(bt_state)]);
  drw_text(drw, x + w1 + lp, 0, (unsigned int)(w2 + lp), (unsigned int)barh, 0,
           "B", 0);

  return x + sysmods[MOD_NETWORK].width;
}

/* ---------------------------
 * CLICK
 * --------------------------- */
void network_click(int button) {
  if (button == 1) {
    static const char *cmd[] = {"/bin/sh", "-c",
                                "$HOME/.local/bin/menu/connectmenu", NULL};
    spawn(cmd);
  }
}

/* ---------------------------
 * SCROLL (unused)
 * --------------------------- */
void network_scroll(int dir) { (void)dir; }
