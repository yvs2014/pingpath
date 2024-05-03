#ifndef TABS_PPGL_H
#define TABS_PPGL_H

#include "common.h"

t_tab* ppgltab_init(GtkWidget *win);
void ppgl_free(void);
void ppgl_restart(void);
void ppgl_update(void);
void ppgl_refresh(void);

#endif
