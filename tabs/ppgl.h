#ifndef TABS_PPGL_H
#define TABS_PPGL_H

#include "common.h"

t_tab* ppgltab_init(GtkWidget *win);
//void graphtab_free(gboolean finish);
void ppgl_update(gboolean retrieve);
void ppgl_final_update(void);
//void graphtab_graph_refresh(gboolean pause_toggled);
//void graphtab_full_refresh(void);

#endif
