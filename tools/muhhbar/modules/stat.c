#define _POSIX_C_SOURCE 200809L
#include "common.h"

static int stat_mode = 0;
static float stat_cpu_tmp = 0.0f;
static float stat_ram_pct = 0.0f;
static char temp_path[128] = "";

static void find_temp_zone(void) {
  char path[128], type[64];
  int z;
  for (z = 0; z < 20; z++) {
    snprintf(path, sizeof path, "/sys/class/thermal/thermal_zone%d/type", z);
    FILE *f = fopen(path, "r");
    if (!f)
      break;
    type[0] = '\0';
    (void)fgets(type, sizeof type, f);
    fclose(f);
    if (strstr(type, "x86_pkg") || strstr(type, "acpitz")) {
      snprintf(temp_path, sizeof temp_path,
               "/sys/class/thermal/thermal_zone%d/temp", z);
      return;
    }
  }
  snprintf(temp_path, sizeof temp_path,
           "/sys/class/thermal/thermal_zone0/temp");
}

void stat_update(void) {
  if (!temp_path[0])
    find_temp_zone();

  int raw = read_int_file(temp_path);
  stat_cpu_tmp = raw / 1000.0f;

  FILE *f = fopen("/proc/meminfo", "r");
  if (f) {
    long total = 0, avail = 0;
    char key[64];
    long val;
    char unit[8];
    while (fscanf(f, "%63s %ld %7s\n", key, &val, unit) >= 2) {
      if (!strcmp(key, "MemTotal:"))
        total = val;
      if (!strcmp(key, "MemAvailable:"))
        avail = val;
      if (total && avail)
        break;
    }
    fclose(f);
    stat_ram_pct =
        total > 0 ? (float)(total - avail) / (float)total * 100.0f : 0.0f;
  }
}

int stat_draw(int x) {
  char text[16];
  int sch = SchNorm;

  if (stat_mode == 0) {
    snprintf(text, sizeof text, "θ %.0f°", stat_cpu_tmp);
    if (stat_cpu_tmp >= 85.0f)
      sch = SchCrit;
    else if (stat_cpu_tmp >= 70.0f)
      sch = SchWarn;
  } else {
    snprintf(text, sizeof text, "μ %.0f%%", stat_ram_pct);
    if (stat_ram_pct >= 90.0f)
      sch = SchCrit;
    else if (stat_ram_pct >= 75.0f)
      sch = SchWarn;
  }

  drw_setscheme(drw, scheme[sch]);
  return drw_text(drw, x, 0, (unsigned int)sysmods[MOD_STAT].width,
                  (unsigned int)barh, (unsigned int)(lrpad / 2), text, 0);
}

void stat_click(int button) {
  if (button == 1) {
    stat_mode ^= 1;
  } else if (button == 3) {
    const char *cmd_cpu[] = {
        "st",
        "-e",
        "sh",
        "-c",
        "ps aux --sort=-%cpu | head -11; printf 'press enter...'; read l",
        NULL};
    const char *cmd_ram[] = {
        "st",
        "-e",
        "sh",
        "-c",
        "ps aux --sort=-%mem | head -11; printf 'press enter...'; read l",
        NULL};
    spawn(stat_mode == 0 ? cmd_cpu : cmd_ram);
  }
}

void stat_scroll(int dir) { (void)dir; }
