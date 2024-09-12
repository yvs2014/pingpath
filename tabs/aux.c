
#include "aux.h"
#include "ui/style.h"
#include "ui/notifier.h"

#define TAB_ELEM_INIT(tabw, maker, cssclass, color) TAB_ELEM_WITH(tabw, maker, cssclass, color, false)

static void tab_aux_reload_css(t_tab_widget *wd, const char *color) {
  if (wd->col)  gtk_widget_remove_css_class(wd->w, wd->col);
  wd->col = color; gtk_widget_add_css_class(wd->w, wd->col);
}

// pub

t_tab* nb_tabs[TAB_NDX_MAX]; // notebook tabs are reorderable

gboolean basetab_init(t_tab *tab, GtkWidget* (*make_dyn)(void), GtkWidget* (*make_extra)(void)) {
  if (!tab || !make_dyn) return false;
  TAB_ELEM_INIT(tab->lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2),      CSS_PAD, NULL);
  TAB_ELEM_INIT(tab->tab, gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN), CSS_PAD, CSS_BGROUND);
  tab->dyn.w = make_dyn(); g_return_val_if_fail(tab->dyn.w, false);
  tab->dyn.col = CSS_BGROUND;
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN); g_return_val_if_fail(box, false);
  gtk_box_append(GTK_BOX(box), tab->dyn.w);
  if (make_extra) { GtkWidget *w = make_extra(); if (GTK_IS_WIDGET(w)) { gtk_box_append(GTK_BOX(box), w); }}
  GtkWidget *scroll = gtk_scrolled_window_new(); g_return_val_if_fail(scroll, false);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(tab->tab.w), scroll);
  return true;
}

gboolean drawtab_init(t_tab *tab, const char *bg, GSList *layers, int ndx) {
  if (!tab || !layers) return false;
  TAB_ELEM_INIT(tab->lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  TAB_ELEM_INIT(tab->dyn, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), NULL,    bg);
  // add overlay
  GtkWidget *over = gtk_overlay_new(); g_return_val_if_fail(over, false);
  for (GSList *l = layers; l; l = l ->next)
    if (l->data) gtk_overlay_add_overlay(GTK_OVERLAY(over), l->data);
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
  t_tab_widget tw[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tw); i++) if (GTK_IS_WIDGET(tw[i].w))
    gtk_widget_set_can_focus(tw[i].w, false);
  if (style_loaded && tab->tab.css && GTK_IS_WIDGET(tab->tab.w))
    gtk_widget_add_css_class(tab->tab.w, tab->tab.css);
}

void tab_color(t_tab *tab) {
  if (!style_loaded || !tab) return;
  t_tab_widget tw[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tw); i++) if (tw[i].col && GTK_IS_WIDGET(tw[i].w))
    tab_aux_reload_css(&tw[i], tw[i].col);
  if (tab->tab.col && GTK_IS_WIDGET(tab->tab.w))
    tab_aux_reload_css(&tab->tab, tab->tab.col);
}

void tab_reload_theme(void) {
  for (int i = 0; i < G_N_ELEMENTS(nb_tabs); i++) if (nb_tabs[i]) tab_color(nb_tabs[i]);
}

#undef TAB_ELEM_INIT

