#ifndef PINGER_H
#define PINGER_H

#include <gtk/gtk.h>
#include "common.h"

#define DEF_COUNT 100
#define DEF_TOUT  1
#define DEF_QOS   0
#define DEF_PPAD "00"
#define DEF_PSIZE 56

typedef struct minmax {
  int min, max;
} t_minmax;

typedef struct opts {
  gchar *target;
  bool dns;
  int count, timeout, qos, size, ipv;
  int min, lim;       // TTL range
  char pad[PAD_SIZE]; // 16 x "00."
  int tout_usec;      // internal const
} t_opts;

typedef struct pinger_state {
  bool run, pause, gotdata, reachable;
} t_pinger_state;

extern t_opts opts;
extern t_pinger_state pinger_state;
extern guint stat_timer; // thread ID of stat-view-area updater

void pinger_init(void);
void pinger_start(void);
void pinger_stop(const gchar* reason);
void pinger_stop_nth(int at, const gchar* reason);
void pinger_free(void);
void pinger_free_errors(void);
void pinger_free_nth_error(int nth);
void pinger_clear_data(bool clean);
bool pinger_within_range(int min, int max, int got);
void pinger_on_quit(bool andstop);

#endif
