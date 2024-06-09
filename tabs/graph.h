#ifndef TABS_GRAPH_H
#define TABS_GRAPH_H

#include "aux.h"

t_tab* graphtab_init(GtkWidget *win);
void graphtab_free(void);
void graphtab_restart(void);
void graphtab_update(void);
void graphtab_refresh(void);

#endif
