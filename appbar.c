
#include "appbar.h"
#include "pinger.h"
#include "styles.h"
#include "valid.h"

typedef void (*update_menu_fn)(GMenu *menu);

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

static void on_reset(GSimpleAction *action, GVariant *var, gpointer data) {
  DEBUG("action: %s", g_action_get_name(G_ACTION(action)));
  pinger_clear_data();
}

const GActionEntry action_entries[] = {
  { .name = "act_start", .activate = on_startstop },
  { .name = "act_pause", .activate = on_pauseresume },
  { .name = "act_reset", .activate = on_reset },
  { .name = "act_help" }, // TODO
};

const GActionEntry option_entries[] = { // TODO
  { .name = "opt_cycle" },
  { .name = "opt_interval" },
  { .name = "opt_dns" },
  { .name = "opt_hopinfo" },
  { .name = "opt_stat" },
  { .name = "opt_ttl" },
  { .name = "opt_payload" },
  { .name = "opt_qos" },
  { .name = "opt_size" },
  { .name = "opt_ipv" },
};

static gboolean update_datetime(gpointer label) {
  static char datetime_label[32];
  g_return_val_if_fail(GTK_IS_LABEL(label), true);
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

static void update_dyn_actions(GMenu *menu) {
  g_return_if_fail(G_IS_MENU(menu));
  g_menu_remove_all(menu);
  g_menu_append_item(menu, g_menu_item_new(ping_opts.timer ? "Stop" : "Start",   "app.act_start"));
  g_menu_append_item(menu, g_menu_item_new(ping_opts.pause ? "Resume" : "Pause", "app.act_pause"));
  g_menu_append_item(menu, g_menu_item_new("Reset", "app.act_reset"));
  g_menu_append_item(menu, g_menu_item_new("Help",  "app.act_help"));
}

static void update_dyn_options(GMenu *menu) {
  g_return_if_fail(G_IS_MENU(menu));
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
};

// pub
//
GMenu* update_menu(GMenu *menu, const char* icon, update_menu_fn update_fn) {
  if (menu) update_fn(menu);
  else {
    menu = g_menu_new();
    g_assert(menu);
    update_fn(menu);
    GtkWidget *button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), icon);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), button);
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    g_assert(popover);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
//    if (css_id) gtk_widget_set_name(popover, css_id);
  }
  return menu;
}


void update_actions(void) {
  static GMenu *action_menu, *option_menu;
  if (!action_menu)
    g_action_map_add_action_entries(G_ACTION_MAP(main_app), action_entries, G_N_ELEMENTS(action_entries), NULL);
  if (!option_menu)
    g_action_map_add_action_entries(G_ACTION_MAP(main_app), option_entries, G_N_ELEMENTS(option_entries), NULL);
  action_menu = update_menu(action_menu, "open-menu-symbolic", update_dyn_actions);
  option_menu = update_menu(option_menu, "document-properties-symbolic", update_dyn_options);
}

static void set_target(GtkWidget *widget, GtkWidget *entry) {
  g_return_if_fail(GTK_IS_EDITABLE(entry));
  gchar *target = valid_target(gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (target) {
    g_free(ping_opts.target); ping_opts.target = target;
    LOG("target: %s", target);
  }
}

static void add_target_input(void) {
 static GtkWidget *target_entry;
 if (target_entry) gtk_header_bar_remove(GTK_HEADER_BAR(appbar), target_entry);
 target_entry = gtk_entry_new();
 gtk_entry_set_max_length(GTK_ENTRY(target_entry), MAXHOSTNAME);
 g_signal_connect(target_entry, "activate", G_CALLBACK(set_target), target_entry);
 gtk_entry_set_placeholder_text(GTK_ENTRY(target_entry), "Enter hostname or IP address ...");
 gtk_editable_set_editable(GTK_EDITABLE(target_entry), true);
 gtk_header_bar_pack_start(GTK_HEADER_BAR(appbar), target_entry);
}

//static void add_spacer(GtkWidget *box) {
//  g_return_if_fail(GTK_IS_BOX(box));
//  GtkWidget *spacer = gtk_label_new(NULL);
//  gtk_widget_set_hexpand(spacer, true);
//  gtk_box_prepend(GTK_BOX(box), spacer);
//}

void init_appbar(GtkApplication *app, GtkWidget *win) {
  if (appbar) return;
  g_return_if_fail(GTK_IS_APPLICATION(app));
  g_return_if_fail(GTK_IS_WINDOW(win));
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

