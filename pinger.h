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

#ifdef WITH_PLOT
#define DEF_DARK_PLOT false

#define DEF_RCOL_FROM 77
#define DEF_RCOL_TO   77
#define DEF_GCOL_FROM 230
#define DEF_GCOL_TO   77
#define DEF_BCOL_FROM 77
#define DEF_BCOL_TO   230

#define DEF_RGLOBAL   true
#define GL_ANGX       0
#define GL_ANGY     -20
#define GL_ANGZ       0
#define DEF_ANGSTEP   5

#define DEF_FOV      45
#endif

typedef struct opts {
  char *target;
  gboolean dns, whois, legend, darktheme, darkgraph;
  int cycles, timeout, qos, size, ipv, graph;
  int min, lim;       // TTL range
  char pad[PAD_SIZE]; // 16 x "00."
  char recap;         // type of summary at exit
  int tout_usec;      // internal: timeout in usecs
  int logmax;         // internal: max lines in log tab
#ifdef WITH_PLOT
  gboolean plot, darkplot;
  t_minmax rcol, gcol, bcol; // color range
  gboolean rglob;            // rotation space: global (xyz) or local (attitude)
  int orient[3];             // plot orientation
  int angstep;               // rotation step
  int fov;                   // field of view
#endif
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

#ifdef WITH_PLOT
#define OPTS_DRAW_REL (opts.graph || opts.plot)
#define DRAW_UPDATE_REL  { if (opts.graph) graphtab_update();  if (opts.plot) plottab_update(); }
#define DRAW_REFRESH_REL { if (opts.graph) graphtab_refresh(); if (opts.plot) plottab_refresh(PL_PARAM_ALL); }
#define DRAW_FREE_REL    { graphtab_free(); plottab_free(); }
#else
#define OPTS_DRAW_REL (opts.graph)
#define DRAW_UPDATE_REL  { if (opts.graph) graphtab_update(); }
#define DRAW_REFRESH_REL { if (opts.graph) graphtab_refresh(); }
#define DRAW_FREE_REL    { graphtab_free(); }
#endif

#endif
