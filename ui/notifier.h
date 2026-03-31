#ifndef UI_NOTIFIER_H
#define UI_NOTIFIER_H

#include <gtk/gtk.h>

enum { NT_MAIN_NDX, NT_LEGEND_NDX,
#ifdef WITH_PLOT
  NT_ROTOR_NDX,
#endif
};

GtkWidget* notifier_init(guint ndx, GtkWidget *base);
void notifier_inform(const char *fmt, ...);
gboolean notifier_get_visible(guint ndx);
void notifier_set_visible(guint ndx, gboolean visible);
void notifier_set_autohide_sec(guint seconds);
void notifier_legend_vis_rows(int upto);
void notifier_legend_update(void);
void notifier_legend_refresh(void);

extern gboolean nt_dark;
extern guint lgnd_excl_mask;

#endif
