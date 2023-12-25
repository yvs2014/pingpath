
#include "appbar.h"
#include "style.h"
#include "action.h"
#include "option.h"
#include "pinger.h"
#include "parser.h"
#include "notifier.h"

#define ENTER_HINT "Enter hostname or IP address ..."

guint datetime_id;

static int update_datetime(gpointer label) {
  static char datetime_label[32]; // note: for 'datetime_id' timer only
  if (datetime_id) {
    bool is_label = GTK_IS_LABEL(label);
    if (!is_label) { datetime_id = 0; g_return_val_if_fail(is_label, G_SOURCE_REMOVE); }
    time_t now = time(NULL);
    strftime(datetime_label, sizeof(datetime_label), "%F %T", localtime(&now));
    gtk_label_set_text(GTK_LABEL(label), datetime_label);
  }
  return datetime_id;
}

static bool start_datetime(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  if (datetime_id) { g_source_remove(datetime_id); datetime_id = 0; }
  GtkWidget *datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(datetime), false);
  if (style_loaded) gtk_widget_set_name(datetime, CSS_ID_DATETIME);
  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(bar), datetime);
  GtkWidget *label = gtk_label_new(NULL);
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
    notifier_inform(ENT_TARGET_HDR " %s", target);
  }
}

static bool add_target_input(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  GtkWidget *entry = gtk_entry_new();
  g_return_val_if_fail(GTK_IS_ENTRY(entry), false);
//gtk_entry_set_has_frame(GTK_ENTRY(entry), false);
  gtk_entry_set_max_length(GTK_ENTRY(entry), MAXHOSTNAME);
  gchar *hint = ENTER_HINT;
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), hint);
  gtk_editable_set_editable(GTK_EDITABLE(entry), true);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(entry), g_utf8_strlen(hint, MAXHOSTNAME) - 5);
  g_signal_connect(entry, EV_ACTIVE, G_CALLBACK(target_cb), entry);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), entry);
  return true;
}


// pub
//

bool appbar_init(GtkApplication *app, GtkWidget *win) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_WINDOW(win), false);
  GtkWidget *bar = gtk_header_bar_new();
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  gtk_window_set_titlebar(GTK_WINDOW(win), bar);
  bool re = true;
  if (!action_init(app, win, bar)) re = false; // mandatory element
  option_init(bar);
  if (!add_target_input(bar)) re = false; // mandatory element
  start_datetime(bar);
  appbar_update();
  return re;
}

void appbar_update(void) { action_update(); option_update(); }

