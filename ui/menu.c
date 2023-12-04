
#include "menu.h"
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

enum { MENU_NDX_ACTION, MENU_NDX_OPTION, MENU_NDX_MAX };
enum { ACT_NDX_START, ACT_NDX_PAUSE, ACT_NDX_RESET, ACT_NDX_HELP, ACT_NDX_MAX };
enum { OPT_NDX_CYCLE, OPT_NDX_INTERVAL, OPT_NDX_DNS, OPT_NDX_HOPINFO, OPT_NDX_STAT,
  OPT_NDX_TTL, OPT_NDX_PAYLOAD, OPT_NDX_QOS, OPT_NDX_SIZE, OPT_NDX_IPV, OPT_NDX_MAX };

static GtkApplication *mainapp;
static GtkWidget *appbar;

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

static const GActionEntry opt_entries[OPT_NDX_MAX] = { // TODO
  [OPT_NDX_CYCLE]    = { .name = "opt_cycle" },
  [OPT_NDX_INTERVAL] = { .name = "opt_interval" },
  [OPT_NDX_DNS]      = { .name = "opt_dns" },
  [OPT_NDX_HOPINFO]  = { .name = "opt_hopinfo" },
  [OPT_NDX_STAT]     = { .name = "opt_stat" },
  [OPT_NDX_TTL]      = { .name = "opt_ttl" },
  [OPT_NDX_PAYLOAD]  = { .name = "opt_payload" },
  [OPT_NDX_QOS]      = { .name = "opt_qos" },
  [OPT_NDX_SIZE]     = { .name = "opt_size" },
  [OPT_NDX_IPV]      = { .name = "opt_ipv" },
};
static GSimpleAction *opt_actions[OPT_NDX_MAX] = {};

static t_menu menus[] = {
  [MENU_NDX_ACTION] = { .n = G_N_ELEMENTS(act_entries), .ent = act_entries, .act = act_actions,
    .update = menu_update_action, .icon = "open-menu-symbolic" },
  [MENU_NDX_OPTION] = { .n = G_N_ELEMENTS(opt_entries), .ent = opt_entries, .act = opt_actions,
    .update = menu_update_option, .icon = "document-properties-symbolic" },
};

// aux
//

#define SET_SA(act, cond) {if (G_IS_SIMPLE_ACTION(act)) g_simple_action_set_enabled(act, cond);}
#define SET_ACT_SA(ndx, cond) SET_SA(act_actions[ndx], cond)
#define SET_OPT_SA(ndx, cond) SET_SA(opt_actions[ndx], cond)

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

static GMenu* create_menu(const char* icon) {
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


// pub
//

bool menu_init(GtkApplication *app, GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  mainapp = app;
  appbar = bar;
  return menu_update();
}

bool menu_update(void) {
  g_return_val_if_fail(GTK_IS_APPLICATION(mainapp), false);
  bool re = true;
  for (int i = 0; i < G_N_ELEMENTS(menus); i++) {
    t_menu *m = &menus[i];
    if (!m->menu) { // create once at first call
      GActionMap *am = G_ACTION_MAP(mainapp);
      g_action_map_add_action_entries(am, m->ent, m->n, NULL);
      for (int j = 0; j < m->n; j++) m->act[j] = G_SIMPLE_ACTION(g_action_map_lookup_action(am, m->ent[j].name));
      m->menu = create_menu(m->icon);
      if (!m->menu) re = false;
    }
    m->update();
  }
  return re;
}

void menu_update_action(void) {
  GMenu* menu = menus[MENU_NDX_ACTION].menu;
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

void menu_update_option(void) {
  GMenu* menu = menus[MENU_NDX_OPTION].menu;
  if (G_IS_MENU(menu)) { // not NULL
    g_menu_remove_all(menu);
    g_menu_append_item(menu, g_menu_item_new("Cycles",        "app.opt_cycle"));
    g_menu_append_item(menu, g_menu_item_new("Interval, sec", "app.opt_interval"));
    g_menu_append_item(menu, g_menu_item_new("DNS",           "app.opt_dns"));
    g_menu_append_item(menu, g_menu_item_new("Hop Info",      "app.opt_hopinfo"));
    g_menu_append_item(menu, g_menu_item_new("Statistics",    "app.opt_stat"));
    g_menu_append_item(menu, g_menu_item_new("TTL",           "app.opt_ttl"));
    g_menu_append_item(menu, g_menu_item_new("Payload, hex",  "app.opt_payload"));
    g_menu_append_item(menu, g_menu_item_new("QoS",           "app.opt_qos"));
    g_menu_append_item(menu, g_menu_item_new("Size",          "app.opt_size"));
    g_menu_append_item(menu, g_menu_item_new("IP Version №",  "app.opt_ipv"));
  }
  bool run = pinger_state.run;
  SET_OPT_SA(OPT_NDX_CYCLE,    !run);
  SET_OPT_SA(OPT_NDX_INTERVAL, !run);
  SET_OPT_SA(OPT_NDX_DNS,      !run);
  SET_OPT_SA(OPT_NDX_TTL,      !run);
  SET_OPT_SA(OPT_NDX_PAYLOAD,  !run);
  SET_OPT_SA(OPT_NDX_QOS,      !run);
  SET_OPT_SA(OPT_NDX_SIZE,     !run);
  SET_OPT_SA(OPT_NDX_IPV,      !run);
}

