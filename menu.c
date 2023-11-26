
#include "menu.h"
#include "pinger.h"

static void on_ping_click(GSimpleAction *simple, GVariant *parameter, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(simple)));
  if (ping_opts.target) {
    if (ping_opts.finish) pinger_start();
    else pinger_stop("request");
  }
}

static void on_quit_click(GSimpleAction *simple, GVariant *parameter, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(simple)));
  pinger_stop("quit");
  LOG("%s", "quit");
  if (widgets.app) g_application_quit(widgets.app);
}

const GActionEntry entries[] = {
  {"startstop", on_ping_click, NULL, NULL, NULL, {0,0,0}},
  {"close",     on_quit_click, NULL, NULL, NULL, {0,0,0}}
};

void update_menu(void) {
  if (!widgets.appbar) return;
  if (widgets.menu) gtk_box_remove(GTK_BOX(widgets.appbar), widgets.menu);
  GMenu* bar = g_menu_new();
  GMenu* action_menu = g_menu_new();
  g_menu_append_submenu(bar, "Action", G_MENU_MODEL(action_menu));
  GMenuItem *item;
  //
  item = g_menu_item_new(ping_opts.finish ? "Start" : "Stop", "app.startstop");
  g_menu_append_item(action_menu, item);
  item = g_menu_item_new("Quit",  "app.close");
  g_menu_append_item(action_menu, item);
  //
  g_action_map_add_action_entries(G_ACTION_MAP(widgets.app), entries, G_N_ELEMENTS(entries), NULL);
  widgets.menu = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
  gtk_box_append(GTK_BOX(widgets.appbar), widgets.menu);
}

