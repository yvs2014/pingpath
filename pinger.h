#ifndef PINGER_H
#define PINGER_H

#include <gtk/gtk.h>
#include "common.h"

#define DEF_QOS   0
#define DEF_PPAD "00"
#define DEF_PSIZE 56

typedef struct ping_opts {
  gchar *target;
  int count, timeout, qos, size, ipv;
  bool dns;
  char pad[48];        // 16 x "00."
  guint timer;         // thread ID of stat-view-area updater
  long long tout_usec; // internal const
} t_ping_opts;

//typedef struct procdata {
//  GSubprocess *proc;
//  bool active;        // process state
//  GString *out, *err; // for received input data and errors
//  int ndx;            // index in internal list
//} t_procdata;

typedef struct pinger_state {
  bool run, pause, gotdata, reachable;
} t_pinger_state;

extern t_ping_opts  ping_opts;
extern t_pinger_state pinger_state;

void pinger_init(void);
void pinger_start(void);
void pinger_stop(const gchar* reason);
void pinger_stop_nth(int at, const gchar* reason);
//bool pinger_active(void);
void pinger_free(void);
void pinger_free_errors(void);
void pinger_free_nth_error(int nth);
void pinger_clear_data(void);

#endif
