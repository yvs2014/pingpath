
#include <gtk/gtk.h>

#include "ui/appbar.h"
#include "ui/styles.h"
#include "tabs/ping.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"

static void on_app_exit(GtkWidget *widget, gpointer unused) {
  pinger_free();
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_stop("at app exit");
  LOG("app %s", "quit");
}

static void activate(GtkApplication* app, gpointer unused) {
  init_css_styles();
  GtkWidget *win = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(win), 1280, 720);
  if (css_loaded) gtk_widget_set_name(win, CSS_ID_WIN);
  init_appbar(app, win);
  t_area *area = init_pingtab();
  if (area->tab) gtk_window_set_child(GTK_WINDOW(win), area->tab);
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(on_app_exit), NULL);
  gtk_window_present(GTK_WINDOW(win));
  if (area->hdr) gtk_list_box_unselect_all(GTK_LIST_BOX(area->hdr));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("net.tools." APPNAME, G_APPLICATION_DEFAULT_FLAGS);
  LOG("app %s", "run");
  init_stat();
  init_pinger();
  init_parser();
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

