#ifndef CONFIG_H
#define CONFIG_H

/* update intervals in seconds */
static const int iv_battery = 15;
static const int iv_brightness = 2;
static const int iv_volume = 1;
static const int iv_network = 15;
static const int iv_stat = 3;
static const int iv_time = 1;

/* mode block geometry */
#define BSQW 8
#define BSQH 8
#define BSQGAP 5
#define BPAD 8
#define BLOCK_TOTAL (2 * BPAD + NMODES * BSQW + (NMODES - 1) * BSQGAP)

#endif /* CONFIG_H */
