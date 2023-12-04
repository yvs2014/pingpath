#ifndef TABS_PING_H
#define TABS_PING_H

#include <gtk/gtk.h>

typedef struct area { GtkWidget *tab, *hdr, *dyn; } t_area;

t_area* pingtab_init(void);
gboolean pingtab_update(gpointer data);
void pingtab_clear(void);
void pingtab_set_visible(int no);
void pingtab_update_width(int max, int ndx);
void pingtab_set_error(const gchar *error);

extern const gchar *info_mesg;
extern const gchar *notyet_mesg;

#endif
