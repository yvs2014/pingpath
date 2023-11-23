
#include <gtk/gtk.h>

#include "menu.h"
#include "pinger.h"
#include "common.h"

t_widgets widgets; // aux widgets' references

static void on_app_exit(GtkWidget *w, gpointer data) {
  LOG("%s", "exit");
  free_ping_errors();
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_stop(data, "app exit");
}

static void activate(GtkApplication* app, gpointer user_data) {
  widgets.app = G_APPLICATION(app);
  widgets.win = gtk_application_window_new(app);
  pingproc.opts.target = "localhost2";
  gtk_window_set_title(GTK_WINDOW(widgets.win), "pingpath");
  gtk_window_set_default_size(GTK_WINDOW(widgets.win), 1280, 768);

  GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(root_box, GTK_ALIGN_START);
  gtk_widget_set_valign(root_box, GTK_ALIGN_START);
  gtk_window_set_child(GTK_WINDOW(widgets.win), root_box);

  widgets.appbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append(GTK_BOX(root_box), widgets.appbar);
  widgets.dyn = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(root_box), widgets.dyn);
  for (int i = 0; i < 2/*MAXTTL*/; i++) {
    pinglines[i] = gtk_label_new(NULL);
    gtk_box_append(GTK_BOX(widgets.dyn), pinglines[i]);
  }
  errline = gtk_label_new(NULL);
  gtk_box_append(GTK_BOX(widgets.dyn), errline);

//  GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
//  gtk_box_append(GTK_BOX(root_box), bottom_bar);
  g_signal_connect_swapped(widgets.win, "destroy", G_CALLBACK(on_app_exit), &pingproc);
  update_menu(&pingproc);
  gtk_window_present(GTK_WINDOW(widgets.win));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("net.tools.pingpath", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int rc = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return rc;
}

