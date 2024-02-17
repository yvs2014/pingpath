#ifndef TABS_GRAPH_H
#define TABS_GRAPH_H

#include "common.h"

t_tab* graphtab_init(GtkWidget *win);
void graphtab_free(gboolean finish);
void graphtab_update(gboolean retrieve);
void graphtab_final_update(void);
void graphtab_toggle_legend(void);
void graphtab_graph_refresh(gboolean pause_toggled);
void graphtab_full_refresh(void);

#endif
