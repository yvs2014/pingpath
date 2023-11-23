
#include "menu.h"
#include "pinger.h"

static void on_ping_click(GSimpleAction *simple, GVariant *parameter, gpointer data) {
  LOG2("action: %s", g_action_get_name(G_ACTION(simple)));
  t_procdata *p = data;
  if (p && p->opts.target) {
    if (p->active) stop_proc(p, "request");
    else start_pings(p, on_output);
  }
}

static void on_quit_click(GSimpleAction *simple, GVariant *parameter, gpointer data) {
  LOG2("action: %s", g_action_get_name(G_ACTION(simple)));
  t_procdata *p = data;
  if (p) {
    if (p->active) stop_proc(p, "quit");
    LOG("%s", "quit");
    if (widgets.app) g_application_quit(widgets.app);
  }
}

const GActionEntry entries[] = {
  {"startstop", on_ping_click, NULL, NULL, NULL, {0,0,0}},
  {"close",     on_quit_click, NULL, NULL, NULL, {0,0,0}}
};

void update_menu(t_procdata *p) {
  if (!p || !widgets.appbar) return;
  if (widgets.menu) gtk_box_remove(GTK_BOX(widgets.appbar), widgets.menu);
  GMenu* bar = g_menu_new();
  GMenu* action_menu = g_menu_new();
  g_menu_append_submenu(bar, "Action", G_MENU_MODEL(action_menu));
  GMenuItem *item;
  //
  item = g_menu_item_new(p->active ? "Stop" : "Start", "app.startstop");
  g_menu_append_item(action_menu, item);
  item = g_menu_item_new("Quit",  "app.close");
  g_menu_append_item(action_menu, item);
  //
  g_action_map_add_action_entries(G_ACTION_MAP(widgets.app), entries, G_N_ELEMENTS(entries), p);
  widgets.menu = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(bar));
  gtk_box_append(GTK_BOX(widgets.appbar), widgets.menu);
}

