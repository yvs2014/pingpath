#ifndef UI_APPBAR_H
#define UI_APPBAR_H

#include <gtk/gtk.h>

bool appbar_init(GtkApplication *app, GtkWidget *win);
extern guint datetime_id;

#endif
