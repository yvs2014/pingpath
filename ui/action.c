
#include "action.h"
#include "appbar.h"
#include "pinger.h"
#include "tabs/ping.h"

#define ACT_MENU_ICON "open-menu-symbolic"

enum { ACT_NDX_START, ACT_NDX_PAUSE, ACT_NDX_RESET, ACT_NDX_HELP, ACT_NDX_MAX };

typedef struct t_act_desc {
  GSimpleAction* sa;
  const char *name;
  const char *const *shortcut;
} t_act_desc;

static const char* cntr_s[] = {"<Control>s", NULL};
static const char* cntr_p[] = {"<Control>p", NULL};
static const char* cntr_r[] = {"<Control>r", NULL};
static const char* cntr_h[] = {"<Control>h", NULL};

static t_act_desc act_desc[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .name = "app.act_start", .shortcut = cntr_s },
  [ACT_NDX_PAUSE] = { .name = "app.act_pause", .shortcut = cntr_p },
  [ACT_NDX_RESET] = { .name = "app.act_reset", .shortcut = cntr_r },
  [ACT_NDX_HELP]  = { .name = "app.act_help",  .shortcut = cntr_h },
};

#define APP_DOT 4
#define SET_SA(ndx, cond) {if (G_IS_SIMPLE_ACTION(act_desc[ndx].sa)) g_simple_action_set_enabled(act_desc[ndx].sa, cond);}

static void on_startstop  (GSimpleAction*, GVariant*, gpointer);
static void on_pauseresume(GSimpleAction*, GVariant*, gpointer);
static void on_reset      (GSimpleAction*, GVariant*, gpointer);
static void on_help       (GSimpleAction*, GVariant*, gpointer);

static GActionEntry act_entries[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .activate = on_startstop },
  [ACT_NDX_PAUSE] = { .activate = on_pauseresume },
  [ACT_NDX_RESET] = { .activate = on_reset },
  [ACT_NDX_HELP]  = { .activate = on_help },
};

static GtkApplication *mainapp;
static GtkWidget *appbar;
static GMenu *act_menu;


// aux
//

static const char* action_label(int ndx) {
  switch (ndx) {
    case ACT_NDX_START: return pinger_state.run ? "Stop" : "Start";
    case ACT_NDX_PAUSE: return pinger_state.pause ? "Resume" : "Pause";
    case ACT_NDX_RESET: return "Reset";
    case ACT_NDX_HELP:  return "Help";
  }
  return "";
}

static void on_startstop(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action: %s", action_label(ACT_NDX_START));
  if (ping_opts.target) {
    if (!ping_opts.timer) pinger_start();
    else pinger_stop("request");
    appbar_update();
  }
}

static void on_pauseresume(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action: %s", action_label(ACT_NDX_PAUSE));
  pinger_state.pause = !pinger_state.pause;
  action_update();
  pingtab_update(NULL);
}

static void on_reset(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action: %s", action_label(ACT_NDX_RESET));
  pinger_clear_data();
}

static void on_help(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action: %s", action_label(ACT_NDX_HELP));
  // TODO
}

static GMenu* create_act_menu(void) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(appbar), NULL);
  GMenu *menu = g_menu_new();
  g_return_val_if_fail(G_IS_MENU(menu), NULL);
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), NULL);
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), ACT_MENU_ICON);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), button);
  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_return_val_if_fail(GTK_IS_POPOVER_MENU(popover), NULL);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
  act_menu = menu;
  return act_menu;
}

static bool create_menus(void) {
  for (int i = 0; i < ACT_NDX_MAX; i++) act_entries[i].name = act_desc[i].name + APP_DOT;
  g_action_map_add_action_entries(G_ACTION_MAP(mainapp), act_entries, ACT_NDX_MAX, NULL);
  for (int i = 0; i < ACT_NDX_MAX; i++)
    act_desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(mainapp), act_entries[i].name));
  if (!create_act_menu()) return false;
  for (int i = 0; i < ACT_NDX_MAX; i++)
    gtk_application_set_accels_for_action(mainapp, act_desc[i].name, act_desc[i].shortcut);
  return true;
}


// pub
//

bool action_init(GtkApplication *app, GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  mainapp = app;
  appbar = bar;
  g_return_val_if_fail(create_menus(), false);
  return true;
}

void action_update(void) {
  if (G_IS_MENU(act_menu)) {
    g_menu_remove_all(act_menu);
    for (int i = 0; i < ACT_NDX_MAX; i++)
      g_menu_append_item(act_menu, g_menu_item_new(action_label(i), act_desc[i].name));
  }
  bool run = pinger_state.run, pause = pinger_state.pause;
  SET_SA(ACT_NDX_START, ping_opts.target != NULL);
  SET_SA(ACT_NDX_PAUSE, pause || (!pause && run));
  SET_SA(ACT_NDX_RESET, run);
}

