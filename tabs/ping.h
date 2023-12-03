#ifndef TABS_PING_H
#define TABS_PING_H

#include <gtk/gtk.h>

typedef struct area { GtkWidget *tab, *hdr, *dyn; } t_area;

t_area* init_pingtab(void);
gboolean update_dynarea(gpointer data);
void clear_dynarea(void);
void set_visible_lines(int no);
void update_elem_width(int max, int ndx);
void set_errline(const gchar *s);

#endif
