#ifndef UI_ACTION_H
#define UI_ACTION_H

#include <gtk/gtk.h>

bool action_init(GtkApplication *app, GtkWidget *win, GtkWidget* bar);
void action_update(void);

#endif
