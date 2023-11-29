
#include "appbar.h"
#include "pinger.h"
#include "styles.h"

GtkApplication *main_app;
GtkWidget *appbar;

static void on_startstop(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  if (ping_opts.target) {
    if (!ping_opts.timer) pinger_start();
    else pinger_stop("request");
    update_actions();
  }
}

static void on_pauseresume(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  ping_opts.pause = !ping_opts.pause;
  update_actions();
}

const GActionEntry entries[] = {
  {"startstop", on_startstop, NULL, NULL, NULL},
  {"pauseresume", on_pauseresume, NULL, NULL},
  {"reset", NULL, NULL, NULL},
  {"help", NULL, NULL, NULL}
};

static gboolean update_datetime(gpointer label) {
  static char datetime_label[32];
  time_t now = time(NULL);
  strftime(datetime_label, sizeof(datetime_label), "%F %T", localtime(&now));
  gtk_label_set_text(GTK_LABEL(label), datetime_label);
  return true;
}

static void start_datetime(void) {
  static GtkWidget *datetime, *label;
  if (datetime) return;
  datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_assert(datetime);
  gtk_widget_set_name(datetime, CSS_ID_DATETIME);
  gtk_header_bar_set_title_widget(GTK_HEADER_BAR(appbar), datetime);
  label = gtk_label_new(NULL);
  g_assert(label);
  gtk_box_append(GTK_BOX(datetime), label);
  gtk_widget_set_visible(label, true);
  g_timeout_add(1000, update_datetime, label);
}

static void update_dyn_menuitems(GMenu *menu, bool init) {
  if (!init) {
    g_menu_remove(menu, 0);
    g_menu_remove(menu, 0);
  }
  g_menu_prepend_item(menu, g_menu_item_new(ping_opts.pause ? "Resume" : "Pause", "app.pauseresume"));
  g_menu_prepend_item(menu, g_menu_item_new(ping_opts.timer ? "Stop" : "Start", "app.startstop"));
}

// pub
//
void update_actions(void) {
  static GtkWidget *action_bar;
  static GMenu* action_menu;
  if (action_menu) update_dyn_menuitems(action_menu, false);
  else {
    action_menu = g_menu_new();
    g_assert(action_menu);
    update_dyn_menuitems(action_menu, true);
    g_menu_append_item(action_menu, g_menu_item_new("Reset",  "app.reset"));
    g_menu_append_item(action_menu, g_menu_item_new("Help",  "app.help"));
    GMenu* bar = g_menu_new();
    g_assert(bar);
    g_menu_append_submenu(bar, "Action", G_MENU_MODEL(action_menu));
    g_action_map_add_action_entries(G_ACTION_MAP(main_app), entries, G_N_ELEMENTS(entries), main_app);
    action_bar = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
    g_assert(action_bar);
    gtk_widget_set_name(action_bar, CSS_ID_MENU);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), action_bar);
  }
}

static void set_target(GtkWidget *widget, GtkWidget *entry) {
  const gchar *target;
  target = gtk_editable_get_text(GTK_EDITABLE(entry));
  ping_opts.target = g_strdup(target);
  LOG("target: %s", target);
}

static void add_target_input(void) {
 static GtkWidget *target_entry;
 if (target_entry) gtk_header_bar_remove(GTK_HEADER_BAR(appbar), target_entry);
 target_entry = gtk_entry_new();
 gtk_entry_set_max_length(GTK_ENTRY(target_entry), 80);
 g_signal_connect(target_entry, "activate", G_CALLBACK(set_target), target_entry);
 gtk_entry_set_placeholder_text(GTK_ENTRY(target_entry), "Enter hostname or IP address ...");
 gtk_editable_set_editable(GTK_EDITABLE(target_entry), true);
 gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), target_entry);
}

//static void add_spacer(GtkWidget *box) {
//  GtkWidget *spacer = gtk_label_new(NULL);
//  gtk_widget_set_hexpand(spacer, true);
//  gtk_box_prepend(GTK_BOX(box), spacer);
//}

void init_appbar(GtkApplication *app, GtkWidget *win) {
  if (appbar) return;
  main_app = app;
  g_assert(main_app);
  appbar = gtk_header_bar_new();
  g_assert(appbar);
  gtk_widget_set_name(appbar, CSS_ID_APPBAR);
  gtk_window_set_titlebar(GTK_WINDOW(win), appbar);
  update_actions();
  add_target_input();
  start_datetime();
}

