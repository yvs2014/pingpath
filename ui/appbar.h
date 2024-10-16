#ifndef UI_APPBAR_H
#define UI_APPBAR_H

#include <gtk/gtk.h>

gboolean appbar_init(GtkApplication *app, GtkWidget *win);
void appbar_update(void);
extern unsigned datetime_id;

#endif
