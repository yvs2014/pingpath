#ifndef PINGER_H
#define PINGER_H

#include "common.h"

#define DEF_CYCLES 100
#define DEF_TOUT   1
#define DEF_QOS    0
#define DEF_PPAD  "00"
#define DEF_PSIZE  56

typedef struct opts {
  gchar *target;
  gboolean dns, whois, legend;
  int cycles, timeout, qos, size, ipv, graph;
  int min, lim;       // TTL range
  char pad[PAD_SIZE]; // 16 x "00."
  char recap;         // type of summary at exit
  int tout_usec;      // internal: timeout in usecs
  int logmax;         // internal: max lines in log tab
} t_opts;

typedef struct pinger_state {
  gboolean run, pause, gotdata, reachable;
} t_pinger_state;

extern t_opts opts;
extern t_pinger_state pinger_state;
extern guint stat_timer; // thread ID of stat-view-area updater

void pinger_init(void);
void pinger_start(void);
void pinger_stop(const gchar* reason);
void pinger_nth_stop(int nth, const gchar* reason);
void pinger_free(void);
void pinger_nth_free_error(int nth);
void pinger_clear_data(gboolean clean);
void pinger_set_error(const gchar *error);
gboolean pinger_within_range(int min, int max, int got);
void pinger_on_quit(gboolean stop);
int pinger_update_tabs(int *pseq);
void pinger_vis_rows(int no);
void pinger_set_width(int typ, int max);
int pinger_recap_cb(GApplication *app);

#endif
