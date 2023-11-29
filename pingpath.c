
#include <gtk/gtk.h>

#include "appbar.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "common.h"
#include "styles.h"

static void on_app_exit(GtkWidget *widget, gpointer unused) {
  pinger_free();
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_stop("app exit");
  LOG("app %s", "quit");
}

static void activate(GtkApplication* app, gpointer unused) {
  static GtkWidget *win;
  win = gtk_application_window_new(app);
  gtk_window_set_default_size(GTK_WINDOW(win), 1280, 768); // TMP
  gtk_widget_set_name(win, CSS_ID_WIN);
  init_appbar(app, win);
  GtkWidget *stats = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN * 2);
  gtk_window_set_child(GTK_WINDOW(win), stats);
  for (int i = 0; i < MAXTTL; i++) {
    pinglines[i] = gtk_label_new(NULL);
    gtk_box_append(GTK_BOX(stats), pinglines[i]);
  }
  errline = gtk_label_new(NULL);
  gtk_box_append(GTK_BOX(stats), errline);
  g_signal_connect_swapped(win, "destroy", G_CALLBACK(on_app_exit), NULL);
  gtk_window_present(GTK_WINDOW(win));
  init_styles();
}

int main(int argc, char **argv) {
  static GtkApplication *app;
  LOG("app %s", "run");
  init_stat();
  init_pinger();
  init_parser();
  app = gtk_application_new("net.tools." APPNAME, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

