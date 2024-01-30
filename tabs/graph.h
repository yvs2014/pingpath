#ifndef TABS_GRAPH_H
#define TABS_GRAPH_H

#include "common.h"

t_tab* graphtab_init(GtkWidget *win);
void graphtab_free(void);
void graphtab_update(gboolean retrieve);
void graphtab_force_update(gboolean pause_toggled);
void graphtab_final_update(void);
void graphtab_toggle_legend(void);
const gchar* graphtab_get_nth_color(int ndx);

#endif
