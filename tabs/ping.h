#ifndef TABS_PING_H
#define TABS_PING_H

#include "common.h"

t_tab* pingtab_init(GtkWidget* win);
void pingtab_update(void);
void pingtab_clear(void);
void pingtab_vis_rows(int no);
void pingtab_vis_cols(void);
void pingtab_set_width(int typ, int max);
void pingtab_set_error(const gchar *error);

#endif
