
#include <gtk/gtk.h>

#include "appbar.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "common.h"
#include "styles.h"

t_widgets widgets; // aux widgets' references

static void on_app_exit(GtkWidget *widget, gpointer unused) {
  LOG("%s", "exit");
  free_ping_errors();
  free_stat();
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_stop(data, "app exit");
}

static void activate(GtkApplication* app, gpointer unused) {
  widgets.app = G_APPLICATION(app);
  widgets.win = gtk_application_window_new(app);
//  ping_opts.target = "localhost";
//  ping_opts.target = "localhost2";
//  ping_opts.target = "192.168.88.100";
//  ping_opts.target = "google.com";
  ping_opts.target = "1.1.1.1";
//  ping_opts.target = "dw.com";
//  ping_opts.target = "yahoo.com"; // TODO: retest first several answers for extra addrnames
  gtk_window_set_title(GTK_WINDOW(widgets.win), "pingpath");
  gtk_window_set_default_size(GTK_WINDOW(widgets.win), 1280, 768);
  gtk_widget_set_name(widgets.win, CSS_ID_WIN);

  GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(widgets.win), root_box);

  widgets.appbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name(widgets.appbar, CSS_ID_APPBAR);
  gtk_box_append(GTK_BOX(root_box), widgets.appbar);
  widgets.stat = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN * 2);
  gtk_box_append(GTK_BOX(root_box), widgets.stat);
  for (int i = 0; i < MAXTTL; i++) {
    pinglines[i] = gtk_label_new(NULL);
    gtk_box_append(GTK_BOX(widgets.stat), pinglines[i]);
  }
  errline = gtk_label_new(NULL);
  gtk_box_append(GTK_BOX(widgets.stat), errline);
//  GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
//  gtk_box_append(GTK_BOX(root_box), bottom_bar);
  g_signal_connect_swapped(widgets.win, "destroy", G_CALLBACK(on_app_exit), NULL);
  init_appbar();
  gtk_window_present(GTK_WINDOW(widgets.win));
  init_styles();
}

int main(int argc, char **argv) {
  init_stat();
  init_pinger();
  init_parser();
  GtkApplication *app = gtk_application_new("net.tools." APPNAME, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

