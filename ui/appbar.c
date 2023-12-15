
#include "appbar.h"
#include "common.h"
#include "style.h"
#include "action.h"
#include "option.h"
#include "pinger.h"
#include "parser.h"

guint datetime_id;
static GtkWidget *appbar;

// aux
//

static gboolean update_datetime(gpointer label) {
  static char datetime_label[32];
  if (!datetime_id) return false;
  time_t now = time(NULL);
  strftime(datetime_label, sizeof(datetime_label), "%F %T", localtime(&now));
  g_return_val_if_fail(GTK_IS_LABEL(label), true);
  gtk_label_set_text(GTK_LABEL(label), datetime_label);
  return datetime_id;
}

static bool start_datetime(void) {
  static GtkWidget *datetime, *label;
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (datetime) return true; // already done
  datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(datetime), false);
  if (style_loaded) gtk_widget_set_name(datetime, CSS_ID_DATETIME);
  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(appbar), datetime);
  label = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), false);
  gtk_box_append(GTK_BOX(datetime), label);
  gtk_widget_set_visible(label, true);
  datetime_id = g_timeout_add(1000, update_datetime, label);
  return true;
}

static void target_cb(GtkWidget *widget, GtkWidget *entry) {
  g_return_if_fail(GTK_IS_EDITABLE(entry));
  gchar *target = parser_valid_target(gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (target) {
    g_free(opts.target); opts.target = target;
    action_update();
    LOG("target: %s", target);
  }
}

static bool add_target_input(void) {
  static GtkWidget *entry;
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (entry) gtk_header_bar_remove(GTK_HEADER_BAR(appbar), entry);
  entry = gtk_entry_new();
  g_return_val_if_fail(GTK_IS_ENTRY(entry), false);
//gtk_entry_set_has_frame(GTK_ENTRY(entry), false);
  gtk_entry_set_max_length(GTK_ENTRY(entry), MAXHOSTNAME);
  gchar *hint = "Enter hostname or IP address ...";
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), hint);
  gtk_editable_set_editable(GTK_EDITABLE(entry), true);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), g_utf8_strlen(hint, MAXHOSTNAME) - 5);
  g_signal_connect(entry, EV_ACTIVE, G_CALLBACK(target_cb), entry);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), entry);
  return true;
}

// pub
//
bool appbar_init(GtkApplication *app, GtkWidget *win) {
  if (appbar) return true; // already done
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  appbar = gtk_header_bar_new();
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), false);
  if (GTK_IS_WINDOW(win)) gtk_window_set_titlebar(GTK_WINDOW(win), appbar);
  if (!action_init(app, appbar)) return false;
  if (!option_init(appbar)) return false;
  if (!add_target_input()) return false;
  if (!start_datetime()) return false;
  appbar_update();
  return true;
}

inline void appbar_update(void) { action_update(); option_update(); }

