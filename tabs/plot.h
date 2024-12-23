#ifndef TABS_PLOT_H
#define TABS_PLOT_H

#include "aux.h"

t_tab* plottab_init(void);
void plottab_free(void);
void plottab_update(void);
void plottab_redraw(void);

enum { PL_PARAM_NONE = 0, PL_PARAM_COLOR = 1, PL_PARAM_TRANS = 2, PL_PARAM_AT = 4, PL_PARAM_FOV = 8,
       PL_PARAM_ALL = PL_PARAM_COLOR | PL_PARAM_TRANS | PL_PARAM_AT | PL_PARAM_FOV};
void plottab_refresh(gboolean flags);
void plottab_on_trans_opts(int quat[4]);

#endif
