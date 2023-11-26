#ifndef __PINGER_H
#define __PINGER_H

#include <gtk/gtk.h>
#include "common.h"

extern t_ping_opts ping_opts;
extern GtkWidget* pinglines[];
extern GtkWidget* errline;

void init_pinger(void);
void pinger_start(void);
void pinger_stop(const gchar* reason);
void stop_ping_at(int at, const gchar* reason);
void clear_errline(void);
void free_ping_error_at(int at);
void free_ping_error_from(int from);
#define free_ping_errors() free_ping_error_from(0)

#endif
