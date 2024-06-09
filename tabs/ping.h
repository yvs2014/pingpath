#ifndef TABS_PING_H
#define TABS_PING_H

#include "aux.h"

t_tab* pingtab_init(GtkWidget* win);
void pingtab_update(void);
void pingtab_clear(void);
void pingtab_vis_rows(int upto);
void pingtab_vis_cols(void);
void pingtab_set_error(const char *error);

#endif
