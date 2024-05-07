#ifndef TABS_PLOT_H
#define TABS_PLOT_H

#include "common.h"

t_tab* plottab_init(GtkWidget *win);
void plottab_free(void);
void plottab_restart(void);
void plottab_update(void);
void plottab_refresh(void);

#endif
