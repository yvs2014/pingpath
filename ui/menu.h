#ifndef UI_MENU_H
#define UI_MENU_H

#include <gtk/gtk.h>

bool menu_init(GtkApplication *app, GtkWidget* bar);
void menu_update(void);
void menu_update_action(void);

#endif
