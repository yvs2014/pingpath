#ifndef TABS_PLOT_H
#define TABS_PLOT_H

#include "aux.h"

#define PL_PARAM_NONE  0U
#define PL_PARAM_COLOR 1U
#define PL_PARAM_TRANS 2U
#define PL_PARAM_AT    4U
#define PL_PARAM_FOV   8U
#define PL_PARAM_ALL   (PL_PARAM_COLOR | PL_PARAM_TRANS | PL_PARAM_AT | PL_PARAM_FOV)

t_tab* plottab_init(void);
void plottab_free(void);
void plottab_update(void);
void plottab_redraw(void);

void plottab_refresh(unsigned flags);
void plottab_on_trans_opts(int quat[4]);

#endif
