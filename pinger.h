#ifndef __PINGER_H
#define __PINGER_H

#include <gtk/gtk.h>
#include "common.h"

#define MAXTTL 30
#define COUNT  10

extern t_procdata pingproc;
extern GtkWidget* pinglines[];

void stop_proc(t_procdata *p, const gchar* reason);
void start_pings(t_procdata *p, GAsyncReadyCallback reset);
void on_output(GObject *stream, GAsyncResult *re, gpointer data);

#endif
