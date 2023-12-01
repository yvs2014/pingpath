#ifndef TABS_PING_H
#define TABS_PING_H

#include <gtk/gtk.h>

GtkWidget* init_pingtab(void);
gboolean update_dynarea(gpointer data);
void clear_dynarea(void);
void set_visible_lines(int no);
void update_elem_width(int max, int ndx);
void set_errline(const gchar *s);

enum { MAX_ELEMS = 3 };
enum ELEM_NDX { NDX_NO, NDX_HOST, NDX_STAT };
typedef gchar* t_elem_array[MAX_ELEMS];                                                                                       
extern const t_elem_array elem_hdr;

#endif
