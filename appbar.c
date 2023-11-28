
#include "appbar.h"
#include "pinger.h"
#include "styles.h"

GtkWidget *datetime;

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
  if (widgets.app) g_application_quit(widgets.app);
}

static gboolean update_datetime(gpointer data) {
  static char datetime_buff[32];
  time_t now = time(NULL);
  strftime(datetime_buff, sizeof(datetime_buff), "%F %T", localtime(&now));
  gtk_label_set_text(GTK_LABEL(datetime), datetime_buff);
  return true;
}

static void start_datetime(void) {
  widgets.datetime = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_name(widgets.datetime, CSS_ID_DATETIME);
  gtk_box_append(GTK_BOX(widgets.appbar), widgets.datetime);
  datetime = gtk_label_new(NULL);
  gtk_box_append(GTK_BOX(widgets.datetime), datetime);
  g_timeout_add(1000, update_datetime, NULL);
  gtk_widget_set_visible(datetime, true);
}

// pub
//
void update_menu(void) {
  if (widgets.menu) gtk_box_remove(GTK_BOX(widgets.appbar), widgets.menu);
  GMenu* bar = g_menu_new();
  GMenu* action_menu = g_menu_new();
  g_menu_append_submenu(bar, "Action", G_MENU_MODEL(action_menu));
  GMenuItem *item;
  //
  item = g_menu_item_new(ping_opts.timer ? "Stop" : "Start", "app.startstop");
  g_menu_append_item(action_menu, item);
  item = g_menu_item_new("Quit",  "app.close");
  g_menu_append_item(action_menu, item);
  //
  g_action_map_add_action_entries(G_ACTION_MAP(widgets.app), entries, G_N_ELEMENTS(entries), NULL);
  widgets.menu = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
  gtk_widget_set_name(widgets.menu, CSS_ID_MENU);
  gtk_box_prepend(GTK_BOX(widgets.appbar), widgets.menu);
}

void init_appbar(void) {
  g_assert(widgets.appbar);
  update_menu();
  //
  GtkWidget *spacer = gtk_label_new(NULL);
  gtk_widget_set_hexpand(spacer, true);
  gtk_box_append(GTK_BOX(widgets.appbar), spacer);
  //
  start_datetime();
}

