#ifndef UI_APPBAR_H
#define UI_APPBAR_H

#include "common.h"

bool appbar_init(GtkApplication *app, GtkWidget *win);
void appbar_update(void);
extern guint datetime_id;

#endif
