#ifndef TABS_PLOT_H
#define TABS_PLOT_H

#include "common.h"

t_tab* plottab_init(GtkWidget *win);
void plottab_free(void);
void plottab_restart(void);
void plottab_update(void);

enum { PL_PARAM_NONE = 0, PL_PARAM_COLOR = 1, PL_PARAM_TRANS = 2,
       PL_PARAM_ALL = PL_PARAM_COLOR | PL_PARAM_TRANS };
void plottab_refresh(gboolean flags);

#endif
