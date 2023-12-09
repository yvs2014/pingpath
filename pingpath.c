
#include <gtk/gtk.h>

#include "ui/appbar.h"
#include "ui/style.h"
#include "tabs/ping.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"

static void on_app_exit(GtkWidget *widget, gpointer unused) {
  g_source_remove(datetime_id); datetime_id = 0; // stop timer unless it's already done
  pinger_free();
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_stop("at app exit");
  LOG("app %s", "quit");
}

static void app_cb(GtkApplication* app, gpointer unused) {
  style_init();
  GtkWidget *win = gtk_application_window_new(app);
  g_return_if_fail(GTK_IS_WINDOW(win));
  gtk_window_set_default_size(GTK_WINDOW(win), 1280, 720);
  if (style_loaded) gtk_widget_add_css_class(win, CSS_BGROUND);
  if (!appbar_init(app, win)) {
    LOG("appbar init %s", "failed");
    g_application_quit(G_APPLICATION(app));
  }
  t_area *area = pingtab_init();
  if (!area) {
    LOG("pingtab init %s", "failed");
    g_application_quit(G_APPLICATION(app));
  }
  if (area->tab) gtk_window_set_child(GTK_WINDOW(win), area->tab);
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(on_app_exit), NULL);
  gtk_window_present(GTK_WINDOW(win));
  if (area->hdr) gtk_list_box_unselect_all(GTK_LIST_BOX(area->hdr));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("net.tools." APPNAME, G_APPLICATION_DEFAULT_FLAGS);
  g_return_val_if_fail(GTK_IS_APPLICATION(app), -1);
  LOG("app %s", "run");
  stat_init(true);
  pinger_init();
  if (!parser_init()) return -1;
  g_signal_connect(app, EV_ACTIVE, G_CALLBACK(app_cb), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

