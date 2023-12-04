
#include "appbar.h"
#include "styles.h"
#include "menu.h"
#include "pinger.h"
#include "valid.h"

static GtkWidget *appbar;

// aux
//

static gboolean update_datetime(gpointer label) {
  static char datetime_label[32];
  time_t now = time(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), true);
  strftime(datetime_label, sizeof(datetime_label), "%F %T", localtime(&now));
  gtk_label_set_text(GTK_LABEL(label), datetime_label);
  return true;
}

static bool start_datetime(void) {
  static GtkWidget *datetime, *label;
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (datetime) return true; // already done
  datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(datetime), false);
  if (styles_loaded) gtk_widget_set_name(datetime, CSS_ID_DATETIME);
  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(appbar), datetime);
  label = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), false);
  gtk_box_append(GTK_BOX(datetime), label);
  gtk_widget_set_visible(label, true);
  g_timeout_add(1000, update_datetime, label);
  return true;
}

static void set_target(GtkWidget *widget, GtkWidget *entry) {
  g_return_if_fail(GTK_IS_EDITABLE(entry));
  gchar *target = valid_target(gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (target) {
    g_free(ping_opts.target); ping_opts.target = target;
    menu_update_action();
    LOG("target: %s", target);
  }
}

static bool add_target_input(void) {
  static GtkWidget *target_entry;
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (target_entry) gtk_header_bar_remove(GTK_HEADER_BAR(appbar), target_entry);
  target_entry = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(target_entry), MAXHOSTNAME);
  g_signal_connect(target_entry, "activate", G_CALLBACK(set_target), target_entry);
  gchar *hint = "Enter hostname or IP address ...";
  gtk_entry_set_placeholder_text(GTK_ENTRY(target_entry), hint);
  gtk_editable_set_editable(GTK_EDITABLE(target_entry), true);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(target_entry), g_utf8_strlen(hint, MAXHOSTNAME) - 5);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), target_entry);
  return true;
}

// pub
//
bool appbar_init(GtkApplication *app, GtkWidget *win) {
  if (appbar) return true; // already done
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  appbar = gtk_header_bar_new();
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (styles_loaded) gtk_widget_set_name(appbar, CSS_ID_APPBAR);
  if (GTK_IS_WINDOW(win)) gtk_window_set_titlebar(GTK_WINDOW(win), appbar);
  if (!menu_init(app, appbar)) return false;
  if (!add_target_input()) return false;
  if (!start_datetime()) return false;
  return true;
}

