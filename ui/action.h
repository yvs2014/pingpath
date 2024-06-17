#ifndef UI_ACTION_H
#define UI_ACTION_H

#include "common.h"
#include "option.h"

gboolean action_init(GtkApplication *app, GtkWidget *win, GtkWidget* bar);
void action_update(void);

enum { ACCL_SA_LGND,
#ifdef WITH_PLOT
  ACCL_SA_LEFT, ACCL_SA_RIGHT, ACCL_SA_UP, ACCL_SA_DOWN, ACCL_SA_PGUP, ACCL_SA_PGDN,
#endif
ACCL_SA_MAX };

#ifdef WITH_PLOT
typedef struct kb_rot_lg { const t_ent_spn_aux *aux; int *pval, sign; gboolean rev; } t_kb_rot_lg;

typedef struct kb_rotaux {
  t_kb_rot_lg global, local;
  int label, *step;
} t_kb_rotaux;
extern t_kb_rotaux kb_rotaux[ACCL_SA_MAX];
void on_rotation(GSimpleAction*, GVariant*, t_kb_rotaux*);
#endif

#endif
