#ifndef SERIES_H
#define SERIES_H

#include "common.h"

#define MIN_SERIES_RANGE 100 // in intervals
#define GRAPH_DATA_GAP   1.1

typedef struct series_list { // keeping len to not calculate it every time
   GSList* list; int len;
} t_series_list;

void series_update(void);
void series_free(gboolean unreg);
void series_lock(void);
void series_unlock(void);
void series_min(int no);
void series_reg_on_scale(GtkWidget *w);

extern t_series_list stat_series[];
#define SERIES(i) stat_series[i].list
#define SERIES_LEN(i) stat_series[i].len
extern const int series_count;
extern int series_datamax;

#define SERIES_LIM ((tgtat > series_count) ? series_count : tgtat)
#define IS_RTT_DATA(d) ((d) && ((d)->rtt > 0))
#define LIMVAL(val, lim) { if (val < 0) val += lim; else if (val >= lim) val -= lim; }

#endif
