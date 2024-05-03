#ifndef PINGER_H
#define PINGER_H

#include "common.h"

#define DEF_DNS    true
#define DEF_WHOIS  true
#define DEF_CYCLES 100
#define DEF_TOUT   1
#define DEF_QOS    0
#define DEF_PPAD  "00"
#define DEF_PSIZE  56
#define DEF_LEGEND     true
#define DEF_DARK_MAIN  true
#define DEF_DARK_GRAPH false
#define DEF_DARK_PPGL  false

typedef struct opts {
  char *target;
  gboolean dns, whois, legend, darktheme, darkgraph;
#ifdef WITH_PPGL
  gboolean ppgl, darkppgl;
#endif
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
extern unsigned stat_timer; // thread ID of stat-view-area updater
extern gboolean atquit;  // got 'destroy' event

void pinger_init(void);
void pinger_cleanup(void);
void pinger_start(void);
void pinger_stop(const char* reason);
void pinger_nth_stop(int nth, const char* reason);
void pinger_nth_free_error(int nth);
void pinger_clear_data(gboolean clean);
void pinger_set_error(const char *error);
gboolean pinger_within_range(int min, int max, int got);
int pinger_update_tabs(int *pseq);
void pinger_vis_rows(int upto);
int pinger_recap_cb(GApplication *app);

#define EN_BOOLPTR(en) ((en)->pval ? (en)->pval : ((en)->valfn ? (en)->valfn((en)->valtype) : NULL))

#ifdef WITH_PPGL
#define OPTS_GRAPH_REL (opts.graph || opts.ppgl)
#define GRAPH_UPDATE_REL { if (opts.graph) graphtab_update(); if (opts.ppgl) ppgl_update(); }
#define GRAPH_REFRESH_REL { if (opts.graph) graphtab_refresh(); if (opts.ppgl) ppgl_refresh(); }
#define GRAPH_RESTART_REL { if (opts.graph) graphtab_restart(); if (opts.ppgl) ppgl_restart(); }
#else
#define OPTS_GRAPH_REL (opts.graph)
#define GRAPH_UPDATE_REL { if (opts.graph) graphtab_update(); }
#define GRAPH_REFRESH_REL { if (opts.graph) graphtab_refresh(); }
#define GRAPH_RESTART_REL { if (opts.graph) graphtab_restart(); }
#endif

#endif
