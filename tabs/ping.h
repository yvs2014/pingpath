#ifndef TABS_PING_H
#define TABS_PING_H

#include "common.h"

t_tab* pingtab_init(void);
gboolean pingtab_update(gpointer data);
void pingtab_wrap_update(void);
void pingtab_clear(void);
void pingtab_vis_rows(int no);
void pingtab_vis_cols(void);
void pingtab_update_width(int max, int ndx);
void pingtab_set_error(const gchar *error);

extern const gchar *info_mesg;
extern const gchar *notyet_mesg;

#endif
