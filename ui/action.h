#ifndef UI_ACTION_H
#define UI_ACTION_H

#include <gtk/gtk.h>

#ifdef WITH_PLOT
#include "ui/option.h"
#endif

gboolean action_init(GtkApplication *app, GtkWidget *win, GtkWidget* bar);
void action_update(void);

enum { ACCL_SA_LGND,
#ifdef WITH_PLOT
  ACCL_SA_LEFT, ACCL_SA_RIGHT, ACCL_SA_UP, ACCL_SA_DOWN, ACCL_SA_PGUP, ACCL_SA_PGDN,
  ACCL_SA_SCUP, ACCL_SA_SCDN,
#endif
ACCL_SA_MAX };

#ifdef WITH_PLOT
typedef struct lg_space { const t_ent_spn_aux *aux; int *pval, sign; gboolean rev; } t_lg_space;

typedef struct kb_plot_aux {
  t_lg_space global, local;
  int label, *step;
} t_kb_plot_aux;
extern t_kb_plot_aux kb_plot_aux[ACCL_SA_MAX];
void on_rotation(GSimpleAction*, GVariant*, t_kb_plot_aux*);
#endif

#endif
