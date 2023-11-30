#ifndef PINGER_H
#define PINGER_H

#include <gtk/gtk.h>
#include "common.h"

extern t_ping_opts ping_opts;
extern GtkWidget* pinglines[];
extern GtkWidget* errline;
extern gchar* ping_errors[];

void init_pinger(void);
void pinger_start(void);
void pinger_stop(const gchar* reason);
void stop_ping_at(int at, const gchar* reason);
void pinger_free(void);
void pinger_clear_data(void);

#endif
