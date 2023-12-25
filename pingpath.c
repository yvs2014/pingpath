
#include "common.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "tabs/ping.h"
#include "tabs/log.h"

static void on_app_exit(GtkWidget *widget, gpointer unused) {
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_on_quit(true);
  pinger_on_quit(false);
  LOG_("app quit");
}

static void on_tab_switch(GtkNotebook *nb, GtkWidget *tab, guint ndx, gpointer unused) {
  if (GTK_IS_BOX(tab)) for (GtkWidget *p = gtk_widget_get_first_child(tab); p; p = gtk_widget_get_next_sibling(p)) {
    if (GTK_IS_LIST_BOX(p)) gtk_list_box_unselect_all(GTK_LIST_BOX(p));
    else if (GTK_IS_SCROLLED_WINDOW(p)) {
      GtkWidget *c = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(p));
      if (GTK_IS_VIEWPORT(c)) c = gtk_viewport_get_child(GTK_VIEWPORT(c));
      if (GTK_IS_BOX(c)) for (GtkWidget *n = gtk_widget_get_first_child(c); n; n = gtk_widget_get_next_sibling(n))
        if (GTK_IS_LIST_BOX(n)) gtk_list_box_unselect_all(GTK_LIST_BOX(n));
    }
  }
}

#define APPQUIT(fmt, ...) { WARN("init " fmt " failed", __VA_ARGS__); \
  g_application_quit(G_APPLICATION(app)); }

static void app_cb(GtkApplication* app, gpointer unused) {
  style_init();
  GtkWidget *win = gtk_application_window_new(app);
  if (!GTK_IS_WINDOW(win)) APPQUIT("%s", "app window");
  gtk_window_set_default_size(GTK_WINDOW(win), 1280, 720);
  if (style_loaded) gtk_widget_add_css_class(win, CSS_BGROUND);
  if (!appbar_init(app, win)) APPQUIT("%s", "appbar");
  GtkWidget *nb = gtk_notebook_new();
  if (!GTK_IS_NOTEBOOK(nb)) APPQUIT("%s", "notebook");
  if (style_loaded) gtk_widget_add_css_class(nb, CSS_BGROUND);
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_BOTTOM);
  t_tab* tabs[TAB_NDX_MAX] = { [TAB_PING_NDX] = pingtab_init(win), [TAB_LOG_NDX] = logtab_init(win) };
  for (int i = 0; i < G_N_ELEMENTS(tabs); i++) {
    t_tab *tab = tabs[i]; if (!tab) APPQUIT("tab#%d", i);
    tab_setup(tab);
    int ndx = gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab->tab, tab->lab);
    if (ndx < 0) APPQUIT("tab page#%d", i);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(nb), tab->tab, true);
  }
  g_signal_connect(nb, EV_TAB_SWITCH, G_CALLBACK(on_tab_switch), NULL);
  // nb overlay
  GtkWidget *over = notifier_init(nb);
  bool with_over = GTK_IS_OVERLAY(over);
  if (!with_over) LOG_("failed to add overlay");
  gtk_window_set_child(GTK_WINDOW(win), with_over ? over : nb);
  //
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(on_app_exit), nb);
  gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("net.tools." APPNAME, G_APPLICATION_DEFAULT_FLAGS);
  g_return_val_if_fail(GTK_IS_APPLICATION(app), -1);
  LOG_("app run");
  stat_init(true);
  pinger_init();
  if (!parser_init()) return -1;
  g_signal_connect(app, EV_ACTIVE, G_CALLBACK(app_cb), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

