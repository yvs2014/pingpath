#ifndef UI_NOTIFIER_H
#define UI_NOTIFIER_H

#include "common.h"

GtkWidget* notifier_init(GtkWidget *base);
void notifier_inform(const gchar *fmt, ...);

#endif
