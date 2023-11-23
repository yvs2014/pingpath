#ifndef __PINGER_H
#define __PINGER_H

#include <gtk/gtk.h>
#include "common.h"

#define MAXTTL 30
#define COUNT  10

extern t_procdata pingproc;
extern GtkWidget* pinglines[];
extern GtkWidget* errline;

void pinger_start(t_procdata *p);
void pinger_stop(t_procdata *p, const gchar* reason);
void clear_errline(void);
void free_ping_errors(void);

#endif
