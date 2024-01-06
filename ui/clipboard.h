#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include "common.h"

gboolean clipboard_init(GtkWidget *win, t_tab *tab);
void cb_on_sall(GSimpleAction *action, GVariant *var, gpointer data);
void cb_on_copy_l1(GSimpleAction *action, GVariant *var, gpointer data);
void cb_on_copy_l2(GSimpleAction *action, GVariant *var, gpointer data);

#endif
