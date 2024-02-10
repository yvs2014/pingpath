#ifndef UI_NOTIFIER_H
#define UI_NOTIFIER_H

#include "common.h"

enum { NT_MAIN_NDX, NT_GRAPH_NDX, NT_NDX_MAX };

GtkWidget* notifier_init(int ndx, GtkWidget *base);
void notifier_inform(const gchar *fmt, ...);
gboolean notifier_get_visible(int ndx);
void notifier_set_visible(int ndx, gboolean visible);
void notifier_legend_vis_rows(int max);
void notifier_legend_update(void);

extern unsigned lgnd_excl_mask;

#endif
