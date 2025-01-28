#ifndef PINGER_H
#define PINGER_H

#include <gtk/gtk.h>

typedef struct pinger_state {
  gboolean run, pause, gotdata, reachable;
} t_pinger_state;

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
gboolean pinger_within_range(int min, int max, int got);
int pinger_update_tabs(int *pseq);
void pinger_vis_rows(int upto);
int pinger_recap_cb(GApplication *app);

#define EN_BOOLPTR(en) ((en)->pval ? (en)->pval : ((en)->valfn ? (en)->valfn((en)->valtype) : NULL))

#endif
