#ifndef TABS_AUX_H
#define TABS_AUX_H

#include "common.h"

typedef struct tab_widget {
  GtkWidget *w;
  const char *css, *col;
} t_tab_widget;

typedef struct t_sa_desc {
  GSimpleAction* sa;
  const char *name;
  const char *const *shortcut;
  void *data;
} t_sa_desc;

typedef struct tab {
  struct tab *self;
  const char *name;
  t_tab_widget tab, lab, dyn, hdr, info;
  const char *tag, *tip, *ico[MAX_ICONS];
  GMenu *menu;       // menu template
  GtkWidget *pop;    // popover menu
  gboolean sel;      // flag of selection
  t_sa_desc desc[POP_MENU_NDX_MAX];
  GActionEntry act[POP_MENU_NDX_MAX];
} t_tab;

gboolean basetab_init(t_tab *tab, GtkWidget* (*make_dyn)(void), GtkWidget* (*make_extra)(void)); // NONNULL(1, 2)
gboolean drawtab_init(t_tab *tab, const char *color, GSList *layers, uint ndx); // NONNULL(1, 3)
void tab_setup(t_tab *tab);
void tab_color(t_tab *tab);
void tab_reload_theme(void);
void tab_dependent(GtkWidget *tab);
#ifdef WITH_PLOT
gboolean is_tab_that(guint ndx);
#endif

extern t_tab* nb_tabs[TAB_NDX_MAX];

void drawtab_free(void);
void drawtab_update(void);
void drawtab_refresh(void);
gboolean need_drawing(void);

#endif
