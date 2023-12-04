#ifndef UI_MENU_H
#define UI_MENU_H

#include <gtk/gtk.h>

bool menu_init(GtkApplication *app, GtkWidget* bar);
bool menu_update(void);
void menu_update_action(void);
void menu_update_option(void);

#endif
