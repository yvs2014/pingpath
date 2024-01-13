#ifndef TABS_GRAPH_H
#define TABS_GRAPH_H

#include "common.h"

t_tab* graphtab_init(GtkWidget *win);
void graphtab_free(void);
void graphtab_update(void);

extern gboolean graph_enable;
extern int graph_type;

#endif
