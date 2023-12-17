
#include "action.h"
#include "common.h"
#include "appbar.h"
#include "pinger.h"
#include "tabs/ping.h"

#define SPANSZ(h, sz, txt) "<span line_height='" h "' size='" sz "'>" txt "</span>\n"
#define SPANHDR(txt)  SPANSZ("1.5", "large",  txt)
#define SPANOPT(l, r) SPANSZ("1.5", "medium", "<b>\t" l "\t</b>" r)
#define SPANSUB(txt)  SPANSZ("1",   "medium", "\t\t<tt>" txt "</tt>")
#define MSTRSTR(x) MSTR(x)
#define MSTR(x) #x

enum { ACT_NDX_START, ACT_NDX_PAUSE, ACT_NDX_RESET, ACT_NDX_HELP, ACT_NDX_QUIT, ACT_NDX_MAX };
enum { APP_NDX, WIN_NDX, APP_WIN_MAX };

typedef struct t_act_desc {
  GSimpleAction* sa;
  const char *name;
  const char *const *shortcut;
} t_act_desc;

static const gchar *help_message =
  SPANHDR("Actions")
  SPANOPT(ACT_START_HDR "/" ACT_STOP_HDR "\t", "target pings")
  SPANOPT(ACT_PAUSE_HDR "/" ACT_RESUME_HDR,    "displaying data")
  SPANOPT(ACT_RESET_HDR  "\t\t", "ping statistics")
  SPANOPT(ACT_QUIT_HDR "\t\t\t", "stop and quit")
  "\n"
  SPANHDR("Options")
  SPANOPT(OPT_CYCLES_HDR   "\t", "Number of ping cycles [" MSTRSTR(DEF_COUNT) "]")
  SPANOPT(OPT_IVAL_HDR     "\t", "Gap in seconds between pings [" MSTRSTR(DEF_TOUT) "]")
  SPANOPT(OPT_DNS_HDR      "\t", "IP address resolving [on]")
  SPANOPT(OPT_INFO_HDR     "\t", "Fields of hop to display:")
    SPANSUB(ELEM_HOST_HDR " " ELEM_AS_HDR " " ELEM_CC_HDR " " ELEM_DESC_HDR " " ELEM_RT_HDR)
  SPANOPT(OPT_STAT_HDR, "Fields of stats to display:")
    SPANSUB(ELEM_LOSS_HDR " " ELEM_SENT_HDR " " ELEM_RECV_HDR " " ELEM_LAST_HDR " " ELEM_BEST_HDR
      " " ELEM_WRST_HDR " " ELEM_AVRG_HDR " " ELEM_JTTR_HDR)
  SPANOPT(OPT_TTL_HDR    "\t\t", "working TTL range [0-" MSTRSTR(MAXTTL) "]")
  SPANOPT(OPT_QOS_HDR      "\t", "QoS/ToS bits of IP header [" MSTRSTR(DEF_QOS) "]")
  SPANOPT(OPT_PLOAD_HDR    "\t", "Up to 16 bytes in hex format [" DEF_PPAD "]")
  SPANOPT(OPT_PSIZE_HDR  "\t\t", "ICMP data size [" MSTRSTR(DEF_PSIZE) "]")
  SPANOPT(OPT_IPV_HDR, "either " OPT_IPVA_HDR ", or " OPT_IPV4_HDR ", or " OPT_IPV6_HDR)
;

static const char* kb_ctrl_s[] = {"<Ctrl>s", NULL};
static const char* kb_space[]  = {"space",   NULL};
static const char* kb_ctrl_r[] = {"<Ctrl>r", NULL};
static const char* kb_ctrl_h[] = {"<Ctrl>h", NULL};
static const char* kb_ctrl_x[] = {"<Ctrl>x", NULL};

static t_act_desc act_desc[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .name = "app.act_start", .shortcut = kb_ctrl_s },
  [ACT_NDX_PAUSE] = { .name = "app.act_pause", .shortcut = kb_space  },
  [ACT_NDX_RESET] = { .name = "app.act_reset", .shortcut = kb_ctrl_r },
  [ACT_NDX_HELP]  = { .name = "app.act_help",  .shortcut = kb_ctrl_h },
  [ACT_NDX_QUIT]  = { .name = "app.act_exit",  .shortcut = kb_ctrl_x },
};

#define APP_DOT 4
#define SET_SA(ndx, cond) {if (G_IS_SIMPLE_ACTION(act_desc[ndx].sa)) g_simple_action_set_enabled(act_desc[ndx].sa, cond);}

static void on_startstop  (GSimpleAction*, GVariant*, gpointer);
static void on_pauseresume(GSimpleAction*, GVariant*, gpointer);
static void on_reset      (GSimpleAction*, GVariant*, gpointer);
static void on_help       (GSimpleAction*, GVariant*, gpointer);
static void on_quit       (GSimpleAction*, GVariant*, gpointer);

static GActionEntry act_entries[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .activate = on_startstop },
  [ACT_NDX_PAUSE] = { .activate = on_pauseresume },
  [ACT_NDX_RESET] = { .activate = on_reset },
  [ACT_NDX_HELP]  = { .activate = on_help },
  [ACT_NDX_QUIT]  = { .activate = on_quit },
};

static GMenu *act_menu;


// aux
//

static const char* action_label(int ndx) {
  switch (ndx) {
    case ACT_NDX_START: return pinger_state.run ? ACT_STOP_HDR : ACT_START_HDR;
    case ACT_NDX_PAUSE: return pinger_state.pause ? ACT_RESUME_HDR : ACT_PAUSE_HDR;
    case ACT_NDX_RESET: return ACT_RESET_HDR;
    case ACT_NDX_HELP:  return ACT_HELP_HDR;
    case ACT_NDX_QUIT:  return ACT_QUIT_HDR;
  }
  return "";
}

static void on_startstop(GSimpleAction *action, GVariant *var, gpointer data) {
  if (opts.target) {
    LOG("action %s", action_label(ACT_NDX_START));
    if (!stat_timer) pinger_start();
    else pinger_stop("request");
    appbar_update();
  }
}

static void on_pauseresume(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action %s", action_label(ACT_NDX_PAUSE));
  pinger_state.pause = !pinger_state.pause;
  action_update();
  pingtab_wrap_update();
}

static void on_reset(GSimpleAction *action, GVariant *var, gpointer data) {
  LOG("action %s", action_label(ACT_NDX_RESET));
  pinger_clear_data(false);
}

static void on_help(GSimpleAction *action, GVariant *var, gpointer data) {
  if (!data) return;
  GtkWidget *win = ((gpointer*)data)[WIN_NDX];
  g_return_if_fail(GTK_IS_WINDOW(win));
  GtkWidget *help = g_object_new(GTK_TYPE_MESSAGE_DIALOG,
    "transient-for", win,
    "destroy-with-parent", true,
    "modal", true,
    "text", appver,
    "secondary-text", help_message,
    "secondary-use-markup", true,
    "buttons", GTK_BUTTONS_OK,
    NULL);
  if (GTK_IS_WINDOW(help)) {
    LOG("action %s", action_label(ACT_NDX_HELP));
    g_signal_connect(help, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(help));
  }
}

static void on_quit(GSimpleAction *action, GVariant *var, gpointer data) {
  if (!data) return;
  GtkWidget *app = ((gpointer*)data)[APP_NDX];
  g_return_if_fail(G_IS_APPLICATION(app));
  LOG("action %s", action_label(ACT_NDX_QUIT));
  pinger_on_quit(true);
  g_application_quit(G_APPLICATION(app));
}

static GMenu* create_act_menu(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), NULL);
  GMenu *menu = g_menu_new();
  g_return_val_if_fail(G_IS_MENU(menu), NULL);
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), NULL);
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), ACT_MENU_ICON);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), button);
  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_return_val_if_fail(GTK_IS_POPOVER_MENU(popover), NULL);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
  act_menu = menu;
  return act_menu;
}

static bool create_menus(GtkApplication *app, GtkWidget *win, GtkWidget *bar) {
  static gpointer appwin[APP_WIN_MAX];
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_WINDOW(win), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  appwin[APP_NDX] = app; appwin[WIN_NDX] = win;
  for (int i = 0; i < ACT_NDX_MAX; i++) act_entries[i].name = act_desc[i].name + APP_DOT;
  g_action_map_add_action_entries(G_ACTION_MAP(app), act_entries, ACT_NDX_MAX, appwin);
  for (int i = 0; i < ACT_NDX_MAX; i++)
    act_desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), act_entries[i].name));
  if (!create_act_menu(bar)) return false;
  for (int i = 0; i < ACT_NDX_MAX; i++)
    gtk_application_set_accels_for_action(app, act_desc[i].name, act_desc[i].shortcut);
  return true;
}


// pub
//

bool action_init(GtkApplication *app, GtkWidget* win, GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_WINDOW(win), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  g_return_val_if_fail(create_menus(app, win, bar), false);
  return true;
}

void action_update(void) {
  if (G_IS_MENU(act_menu)) {
    g_menu_remove_all(act_menu);
    for (int i = 0; i < ACT_NDX_MAX; i++)
      g_menu_append_item(act_menu, g_menu_item_new(action_label(i), act_desc[i].name));
  }
  bool run = pinger_state.run, pause = pinger_state.pause;
  SET_SA(ACT_NDX_START, opts.target != NULL);
  SET_SA(ACT_NDX_PAUSE, pause || (!pause && run));
  SET_SA(ACT_NDX_RESET, run);
}

