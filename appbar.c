
#include "appbar.h"
#include "pinger.h"
#include "styles.h"

GtkApplication *main_app;
GtkWidget *datetime;
GtkWidget *appbar;

static void on_ping_click(GSimpleAction*, GVariant*, gpointer data);
static void on_quit_click(GSimpleAction*, GVariant*, gpointer data);

const GActionEntry entries[] = {
  {"startstop", on_ping_click, NULL, NULL, NULL, {0,0,0}},
  {"close",     on_quit_click, NULL, NULL, NULL, {0,0,0}}
};

static void on_ping_click(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  if (ping_opts.target) {
    if (!ping_opts.timer) pinger_start();
    else pinger_stop("request");
  }
}

static void on_quit_click(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  pinger_stop("quit");
  LOG("%s", "quit");
  if (data) g_application_quit(data);
}

static gboolean update_datetime(gpointer data) {
  GtkWidget *label = data;
  static char datetime_buff[32];
  time_t now = time(NULL);
  strftime(datetime_buff, sizeof(datetime_buff), "%F %T", localtime(&now));
  gtk_label_set_text(GTK_LABEL(label), datetime_buff);
  return true;
}

static void start_datetime(void) {
  static guint datetime_timer_id;
  static GtkWidget *label;
  if (datetime_timer_id) g_source_remove(datetime_timer_id);
  if (label) gtk_box_remove(GTK_BOX(datetime), label);
  if (datetime) gtk_box_remove(GTK_BOX(appbar), datetime);
  datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name(datetime, CSS_ID_DATETIME);
  gtk_box_append(GTK_BOX(appbar), datetime);
  label = gtk_label_new(NULL);
  gtk_box_append(GTK_BOX(datetime), label);
  gtk_widget_set_visible(label, true);
  datetime_timer_id = g_timeout_add(1000, update_datetime, label);
}

// pub
//
void update_menu(void) {
  static GtkWidget *action_bar;
  g_assert(main_app);
  if (action_bar) gtk_box_remove(GTK_BOX(appbar), action_bar);
  GMenu* action_menu = g_menu_new();
  g_menu_append_item(action_menu, g_menu_item_new(ping_opts.timer ? "Stop" : "Start", "app.startstop"));
  g_menu_append_item(action_menu, g_menu_item_new("Quit",  "app.close"));
  GMenu* bar = g_menu_new();
  g_menu_append_submenu(bar, "Action", G_MENU_MODEL(action_menu));
  //
  g_action_map_add_action_entries(G_ACTION_MAP(main_app), entries, G_N_ELEMENTS(entries), main_app);
  action_bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
  gtk_widget_set_name(action_bar, CSS_ID_MENU);
  gtk_box_prepend(GTK_BOX(appbar), action_bar);
}

static void set_target(GtkWidget *widget, GtkWidget *entry) {
  const gchar *target;
  target = gtk_editable_get_text(GTK_EDITABLE(entry));
  ping_opts.target = g_strdup(target);
  LOG("target: %s", target);
}

static void add_target_input(void) {
 static GtkWidget *target_entry;
 if (target_entry) gtk_box_remove(GTK_BOX(appbar), target_entry);
 target_entry = gtk_entry_new();
 gtk_entry_set_max_length(GTK_ENTRY(target_entry), 80);
 g_signal_connect(target_entry, "activate", G_CALLBACK(set_target), target_entry);
 gtk_entry_set_placeholder_text(GTK_ENTRY(target_entry), "Enter hostname or IP address ...");
 gtk_editable_set_editable(GTK_EDITABLE(target_entry), true);
 gtk_box_append(GTK_BOX(appbar), target_entry);
}

static void add_spacer(void) {
  GtkWidget *spacer = gtk_label_new(NULL);
  gtk_widget_set_hexpand(spacer, true);
  gtk_box_append(GTK_BOX(appbar), spacer);
}

void init_appbar(GtkApplication *app, GtkWidget *parent) {
  g_assert(parent);
  g_assert(app);
  main_app = app;
  if (appbar) gtk_box_remove(GTK_BOX(parent), appbar);
  appbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_assert(appbar);
  gtk_widget_set_name(appbar, CSS_ID_APPBAR);
  gtk_box_append(GTK_BOX(parent), appbar);
  //
  update_menu();
  add_target_input();
  add_spacer();
  start_datetime();
}

