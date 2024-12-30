#ifndef UI_NOTIFIER_H
#define UI_NOTIFIER_H

#include <gtk/gtk.h>

enum { NT_MAIN_NDX, NT_LEGEND_NDX,
#ifdef WITH_PLOT
  NT_ROTOR_NDX,
#endif
};

GtkWidget* notifier_init(unsigned ndx, GtkWidget *base);
void notifier_inform(const char *fmt, ...);
gboolean notifier_get_visible(unsigned ndx);
void notifier_set_visible(unsigned ndx, gboolean visible);
void notifier_legend_vis_rows(int upto);
void notifier_legend_update(void);
void notifier_legend_refresh(void);

extern unsigned lgnd_excl_mask;
extern gboolean nt_dark;

#endif
