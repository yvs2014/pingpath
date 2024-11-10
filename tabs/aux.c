
#include <stdlib.h>

#include "config.h"
#include "common.h"

#include "aux.h"
#include "ui/style.h"
#include "ui/notifier.h"
#include "tabs/graph.h"
#ifdef WITH_PLOT
#include "tabs/plot.h"
#endif

static void tab_aux_reload_css(t_tab_widget *tab_widget, const char *color) {
  if (tab_widget->col) gtk_widget_remove_css_class(tab_widget->w, tab_widget->col);
  tab_widget->col = color;
  gtk_widget_add_css_class(tab_widget->w, tab_widget->col);
}

// pub

t_tab* nb_tabs[TAB_NDX_MAX]; // notebook tabs are reorderable

#define TAB_ELEM_INIT(tabw, maker, cssclass, color) TAB_ELEM_WITH(tabw, maker, cssclass, color, false)
gboolean basetab_init(t_tab *tab, GtkWidget* (*make_dyn)(void), GtkWidget* (*make_extra)(void)) {
  if (!tab || !make_dyn) return false;
  TAB_ELEM_INIT(tab->lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2),      CSS_PAD, NULL);
  TAB_ELEM_INIT(tab->tab, gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN), CSS_PAD, CSS_BGROUND);
  tab->dyn.w = make_dyn(); g_return_val_if_fail(tab->dyn.w, false);
  tab->dyn.col = CSS_BGROUND;
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN); g_return_val_if_fail(box, false);
  gtk_box_append(GTK_BOX(box), tab->dyn.w);
  if (make_extra) {
    GtkWidget *widget = make_extra();
    if (GTK_IS_WIDGET(widget)) gtk_box_append(GTK_BOX(box), widget);
  }
  GtkWidget *scroll = gtk_scrolled_window_new(); g_return_val_if_fail(scroll, false);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(tab->tab.w), scroll);
  return true;
}

gboolean drawtab_init(t_tab *tab, const char *color, GSList *layers, int ndx) {
  if (!tab || !layers) return false;
  TAB_ELEM_INIT(tab->lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  TAB_ELEM_INIT(tab->dyn, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), NULL,    color);
  // add overlay
  GtkWidget *over = gtk_overlay_new(); g_return_val_if_fail(over, false);
  for (GSList *list = layers; list; list = list->next)
    if (list->data) gtk_overlay_add_overlay(GTK_OVERLAY(over), list->data);
  gtk_overlay_set_child(GTK_OVERLAY(over), tab->dyn.w);
  // wrap scrolling
  tab->tab.w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); g_return_val_if_fail(tab->tab.w, false);
  GtkWidget *scroll = gtk_scrolled_window_new(); g_return_val_if_fail(scroll, false);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  // put altogether into tab
  GtkWidget *notify = notifier_init(ndx, scroll);
  gtk_box_append(GTK_BOX(tab->tab.w), notify ? notify : scroll);
  return true;
}
#undef TAB_ELEM_INIT

void tab_setup(t_tab *tab) {
  if (!tab) return;
  if (GTK_IS_WIDGET(tab->lab.w)) {
    gtk_widget_set_hexpand(tab->lab.w, true);
    if (style_loaded && tab->lab.css) gtk_widget_add_css_class(tab->lab.w, tab->lab.css);
    if (GTK_IS_BOX(tab->lab.w)) {
      const char *ico = is_sysicon(tab->ico);
      if (!ico) WARN("No icon found for %s", tab->name);
      gtk_box_append(GTK_BOX(tab->lab.w), gtk_image_new_from_icon_name(ico));
      if (tab->tag) gtk_box_append(GTK_BOX(tab->lab.w), gtk_label_new(tab->tag));
      if (tab->tip) gtk_widget_set_tooltip_text(tab->lab.w, tab->tip);
    }
  }
  t_tab_widget tab_widget[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tab_widget); i++)
    if (GTK_IS_WIDGET(tab_widget[i].w))
      gtk_widget_set_can_focus(tab_widget[i].w, false);
  if (style_loaded && tab->tab.css && GTK_IS_WIDGET(tab->tab.w))
    gtk_widget_add_css_class(tab->tab.w, tab->tab.css);
}

void tab_color(t_tab *tab) {
  if (!style_loaded || !tab) return;
  t_tab_widget tab_widget[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tab_widget); i++)
    if (tab_widget[i].col && GTK_IS_WIDGET(tab_widget[i].w))
      tab_aux_reload_css(&tab_widget[i], tab_widget[i].col);
  if (tab->tab.col && GTK_IS_WIDGET(tab->tab.w))
    tab_aux_reload_css(&tab->tab, tab->tab.col);
}

void tab_reload_theme(void) {
  for (int i = 0; i < G_N_ELEMENTS(nb_tabs); i++) if (nb_tabs[i]) tab_color(nb_tabs[i]);
}

inline gboolean need_drawing(void) {
  return opts.graph
#ifdef WITH_PLOT
      || opts.plot
#endif
  ;
}

inline void drawtab_free(void) {
  graphtab_free();
#ifdef WITH_PLOT
  plottab_free();
#endif
}

inline void drawtab_update(void)  {
  if (opts.graph) graphtab_update();
#ifdef WITH_PLOT
  if (opts.plot) plottab_update();
#endif
}

inline void drawtab_refresh(void) {
  if (opts.graph) graphtab_refresh();
#ifdef WITH_PLOT
  if (opts.plot) plottab_refresh(PL_PARAM_ALL);
#endif
}

