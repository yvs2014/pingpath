#ifndef __APPBAR_H
#define __APPBAR_H

#include <gtk/gtk.h>
#include "common.h"

void init_appbar(GtkApplication *app, GtkWidget *parent);
void update_menu(void);

#endif
