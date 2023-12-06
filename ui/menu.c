
#include "menu.h"
#include "option.h"
#include "pinger.h"
#include "tabs/ping.h"

typedef struct menu {
  int n;
  const GActionEntry *ent;
  GSimpleAction **act;
  void (*update)(void);
  gchar *icon;
  GMenu *menu;
} t_menu;

enum { ACT_NDX_START, ACT_NDX_PAUSE, ACT_NDX_RESET, ACT_NDX_HELP, ACT_NDX_MAX };

static GtkApplication *mainapp;
static GtkWidget *appbar;
static bool menu_created;

//

static void on_startstop(GSimpleAction*, GVariant*, gpointer);
static void on_pauseresume(GSimpleAction*, GVariant*, gpointer);
static void on_reset(GSimpleAction*, GVariant*, gpointer);

static const GActionEntry act_entries[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .name = "act_start", .activate = on_startstop },
  [ACT_NDX_PAUSE] = { .name = "act_pause", .activate = on_pauseresume },
  [ACT_NDX_RESET] = { .name = "act_reset", .activate = on_reset },
  [ACT_NDX_HELP]  = { .name = "act_help" }, // TODO
};
static GSimpleAction* act_actions[ACT_NDX_MAX] = {};

static t_menu action_menu = {
  .n = G_N_ELEMENTS(act_entries), .ent = act_entries, .act = act_actions,
  .update = menu_update_action, .icon = "open-menu-symbolic"
};

// aux
//

#define SET_SA(act, cond) {if (G_IS_SIMPLE_ACTION(act)) g_simple_action_set_enabled(act, cond);}
#define SET_ACT_SA(ndx, cond) SET_SA(act_actions[ndx], cond)

static void on_startstop(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  if (ping_opts.target) {
    if (!ping_opts.timer) pinger_start();
    else pinger_stop("request");
    menu_update();
  }
}

static void on_pauseresume(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  pinger_state.pause = !pinger_state.pause;
  menu_update_action();
  pingtab_update(NULL);
}

static void on_reset(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  pinger_clear_data();
}

static GMenu* create_gmenu(const char* icon) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), NULL);
  GMenu *menu = g_menu_new();
  g_return_val_if_fail(G_IS_MENU(menu), NULL);
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), NULL);
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), icon);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), button);
  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_return_val_if_fail(GTK_IS_POPOVER_MENU(popover), NULL);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
  return menu;
}

static void create_menus(void) {
  g_return_if_fail(GTK_IS_APPLICATION(mainapp));
  GActionMap *am = G_ACTION_MAP(mainapp);
  g_action_map_add_action_entries(am, action_menu.ent, action_menu.n, NULL);
  for (int j = 0; j < action_menu.n; j++)
    action_menu.act[j] = G_SIMPLE_ACTION(g_action_map_lookup_action(am, action_menu.ent[j].name));
  action_menu.menu = create_gmenu(action_menu.icon);
  if (!action_menu.menu) return;
  if (!option_init(appbar, "document-properties-symbolic")) return;
  menu_created = true;
}


// pub
//

bool menu_init(GtkApplication *app, GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  mainapp = app;
  appbar = bar;
  create_menus();
  g_return_val_if_fail(menu_created, false);
  menu_update();
  return true;
}

void menu_update(void) {
  if (action_menu.menu) action_menu.update();
  option_update();
}

void menu_update_action(void) {
  GMenu* menu = action_menu.menu;
  if (G_IS_MENU(menu)) { // not NULL
    g_menu_remove_all(menu);
    g_menu_append_item(menu, g_menu_item_new(pinger_state.run ? "Stop" : "Start",   "app.act_start"));
    g_menu_append_item(menu, g_menu_item_new(pinger_state.pause ? "Resume" : "Pause", "app.act_pause"));
    g_menu_append_item(menu, g_menu_item_new("Reset", "app.act_reset"));
    g_menu_append_item(menu, g_menu_item_new("Help",  "app.act_help"));
  }
  SET_ACT_SA(ACT_NDX_START, ping_opts.target != NULL);
  bool run = pinger_state.run, pause = pinger_state.pause;
  SET_ACT_SA(ACT_NDX_PAUSE, pause || (!pause && run));
  SET_ACT_SA(ACT_NDX_RESET, run);
}

