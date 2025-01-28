
#include <stdlib.h>
#include <time.h>

#include "text.h"
#include "common.h"
#include "appbar.h"
#include "style.h"
#include "action.h"
#include "option.h"
#include "pinger.h"
#include "parser.h"
#include "notifier.h"

unsigned datetime_id;

static int update_datetime(void *label) {
  if (atquit) datetime_id = 0;
  if (datetime_id) {
    if (GTK_IS_LABEL(label)) {
      char ts[32];
      gtk_label_set_text(GTK_LABEL(label), timestamp(ts, sizeof(ts)));
      return G_SOURCE_CONTINUE;
    }
    datetime_id = 0;
  }
  return G_SOURCE_REMOVE;
}

static gboolean start_datetime(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  if (datetime_id) { g_source_remove(datetime_id); datetime_id = 0; }
  GtkWidget *datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(datetime), false);
  if (style_loaded) gtk_widget_add_css_class(datetime, CSS_ID_DATETIME);
  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(bar), datetime);
  GtkWidget *label = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), false);
  gtk_box_append(GTK_BOX(datetime), label);
  gtk_widget_set_visible(label, true);
  datetime_id = g_timeout_add_seconds(MAIN_TIMING_SEC, update_datetime, label);
  return true;
}

static void on_target(GtkEntry *entry G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
  g_return_if_fail(GTK_IS_EDITABLE(entry));
  char *target = parser_valid_target(gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (target) {
    g_free(opts.target); opts.target = target;
    action_update();
    notifier_inform("%s %s", ENT_TARGET_HDR, target);
  }
}

static gboolean add_target_input(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  GtkWidget *entry = gtk_entry_new();
  g_return_val_if_fail(GTK_IS_ENTRY(entry), false);
  gtk_entry_set_max_length(GTK_ENTRY(entry), MAXHOSTNAME);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), TARGET_HINT);
  gtk_editable_set_editable(GTK_EDITABLE(entry), true);
  gtk_editable_set_max_width_chars(
    GTK_EDITABLE(entry),
    g_utf8_strlen(TARGET_HINT, MAXHOSTNAME) - 5);
  if (opts.target)
    gtk_editable_set_text(GTK_EDITABLE(entry), opts.target);
  g_signal_connect(entry, EV_ACTIVE, G_CALLBACK(on_target), NULL);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), entry);
  return true;
}


// pub
//

gboolean appbar_init(GtkApplication *app, GtkWidget *win) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_WINDOW(win), false);
  GtkWidget *bar = gtk_header_bar_new();
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  gtk_window_set_titlebar(GTK_WINDOW(win), bar);
  gboolean okay = true;
  if (!action_init(app, win, bar)) okay = false;
  if (!option_init(bar))           okay = false;
  if (!add_target_input(bar))      okay = false;
  start_datetime(bar);
  appbar_update();
  return okay;
}

void appbar_update(void) { action_update(); option_update(); }

