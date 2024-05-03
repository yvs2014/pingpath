
#include "action.h"
#include "option.h"
#include "appbar.h"
#include "notifier.h"
#include "pinger.h"
#include "graph_rel.h"
#include "series.h"
#include "ui/style.h"

#if PANGO_VERSION_MAJOR == 1
#if PANGO_VERSION_MINOR < 50
#define LINE_HEIGHT(f) ""
#else
#define LINE_HEIGHT(f) "line_height='" f "'"
#endif
#endif

#define SPANSZ(h, sz, txt) "<span " LINE_HEIGHT(h) " size='" sz "'>" txt "</span>\n"
#define SPANHDR(txt)  SPANSZ("1.5", "large",  txt)
#define SPANOPT(l, r) SPANSZ("1.5", "medium", "<b>\t" l "\t</b>" r)
#define SPANSUB(txt)  SPANSZ("1",   "medium", "\t\t<tt>" txt "</tt>")
#define MA_LOG(ndx) LOG("Action %s", action_label(ndx))
#define MI_LOG(ndx) notifier_inform("Action %s", action_label(ndx))

#define OKAY_BUTTON "Okay"

typedef struct help_dialog_in {
  GtkWidget *win, *box, *hdr, *scroll, *body, *btn;
} t_help_dialog_in;

enum { ACT_NDX_START, ACT_NDX_PAUSE, ACT_NDX_RESET, ACT_NDX_HELP, ACT_NDX_QUIT, ACT_NDX_LGND, ACT_NDX_MAX };

static const char* kb_ctrl_s[] = {"<Ctrl>s", NULL};
static const char* kb_space[]  = {"space",   NULL};
static const char* kb_ctrl_r[] = {"<Ctrl>r", NULL};
static const char* kb_ctrl_h[] = {"<Ctrl>h", NULL};
static const char* kb_ctrl_x[] = {"<Ctrl>x", NULL};
static const char* kb_ctrl_l[] = {"<Ctrl>l", NULL};

static const char *help_message =
  SPANHDR("Actions")
  SPANOPT(ACT_START_HDR    "/" ACT_STOP_HDR "\t", "target pings")
  SPANOPT(ACT_PAUSE_HDR    "/" ACT_RESUME_HDR,    "displaying data")
  SPANOPT(ACT_RESET_HDR    "\t\t", "ping statistics")
  SPANOPT(ACT_QUIT_HDR     "\t\t\t", "stop and quit")
  "\n"
  SPANHDR("Main Options")
  SPANOPT(OPT_CYCLES_HDR   "\t", "Number of ping cycles [" G_STRINGIFY(DEF_CYCLES) "]")
  SPANOPT(OPT_IVAL_HDR     "\t", "Gap in seconds between pings [" G_STRINGIFY(DEF_TOUT) "]")
  SPANOPT(OPT_DNS_HDR      "\t", "IP address resolving, on | off")
  SPANOPT(OPT_INFO_HEADER  "\t", "Hop elements to display:")
    SPANSUB(ELEM_HOST_HDR  " " ELEM_AS_HDR " " ELEM_CC_HDR " " ELEM_DESC_HDR " " ELEM_RT_HDR)
  SPANOPT(OPT_STAT_HDR,    "Stat elements to display:")
    SPANSUB(ELEM_LOSS_HDR  " " ELEM_SENT_HDR " " ELEM_RECV_HDR " " ELEM_LAST_HDR " " ELEM_BEST_HDR
      " " ELEM_WRST_HDR " " ELEM_AVRG_HDR " " ELEM_JTTR_HDR)
  SPANOPT(OPT_TTL_HDR      "\t\t", "working TTL range [0-" G_STRINGIFY(MAXTTL) "]")
  SPANOPT(OPT_QOS_HDR      "\t", "QoS/ToS bits of IP header [" G_STRINGIFY(DEF_QOS) "]")
  SPANOPT(OPT_PLOAD_HDR    "\t", "Up to 16 bytes in hex format [" DEF_PPAD "]")
  SPANOPT(OPT_PSIZE_HDR    "\t\t", "ICMP data size [" G_STRINGIFY(DEF_PSIZE) "]")
  SPANOPT(OPT_IPV_HDR,     "either " OPT_IPVA_HDR ", or " OPT_IPV4_HDR ", or " OPT_IPV6_HDR)
  "\n"
  SPANHDR("Auxiliary")
  SPANOPT(OPT_MN_DARK_HEADER, "dark | light")
  SPANOPT(OPT_GR_DARK_HEADER, "light | dark")
  SPANOPT(OPT_GRAPH_HDR,   "either " OPT_GR_NONE_HDR ", or " OPT_GR_DOT_HDR ", or " OPT_GR_LINE_HDR ", or " OPT_GR_CURVE_HDR)
  SPANOPT(OPT_GRLG_HDR,    "to display:")
    SPANSUB(GRLG_AVJT_HDR  " " GRLG_CCAS_HDR " " GRLG_LGHN_HDR)
  SPANOPT(OPT_GREX_HDR,    "to display:")
    SPANSUB(GREX_MEAN_HDR  " " GREX_JRNG_HDR " " GREX_AREA_HDR)
  SPANOPT(OPT_LOGMAX_HDR,  "Max rows in log tab [" G_STRINGIFY(DEF_LOGMAX) "]")
  "\n"
  SPANHDR("CLI")
  SPANSZ("1.5", "medium", "\tfor command-line options see <b>pingpath(1)</b> manual page")
;

static t_act_desc act_desc[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .name = "app.act_start", .shortcut = kb_ctrl_s },
  [ACT_NDX_PAUSE] = { .name = "app.act_pause", .shortcut = kb_space  },
  [ACT_NDX_RESET] = { .name = "app.act_reset", .shortcut = kb_ctrl_r },
  [ACT_NDX_HELP]  = { .name = "app.act_help",  .shortcut = kb_ctrl_h },
  [ACT_NDX_QUIT]  = { .name = "app.act_exit",  .shortcut = kb_ctrl_x },
  [ACT_NDX_LGND]  = { .name = "app.act_lgnd",  .shortcut = kb_ctrl_l, .invisible = true },
};

static void on_startstop  (GSimpleAction*, GVariant*, void*);
static void on_pauseresume(GSimpleAction*, GVariant*, void*);
static void on_reset      (GSimpleAction*, GVariant*, void*);
static void on_help       (GSimpleAction*, GVariant*, void*);
static void on_quit       (GSimpleAction*, GVariant*, void*);
static void on_legend     (GSimpleAction*, GVariant*, void*);

static GActionEntry act_entries[ACT_NDX_MAX] = {
  [ACT_NDX_START] = { .activate = on_startstop },
  [ACT_NDX_PAUSE] = { .activate = on_pauseresume },
  [ACT_NDX_RESET] = { .activate = on_reset },
  [ACT_NDX_HELP]  = { .activate = on_help },
  [ACT_NDX_QUIT]  = { .activate = on_quit },
  [ACT_NDX_LGND]  = { .activate = on_legend },
};

static GMenu *action_menu;

static const char* action_label(int ndx) {
  switch (ndx) {
    case ACT_NDX_START: return pinger_state.run ? ACT_STOP_HDR : ACT_START_HDR;
    case ACT_NDX_PAUSE: return pinger_state.pause ? ACT_RESUME_HDR : ACT_PAUSE_HDR;
    case ACT_NDX_RESET: return ACT_RESET_HDR;
    case ACT_NDX_HELP:  return ACT_HELP_HDR;
    case ACT_NDX_QUIT:  return ACT_QUIT_HDR;
    case ACT_NDX_LGND:  return ACT_LGND_HDR;
  }
  return "";
}

static void on_startstop(GSimpleAction *action, GVariant *var, void *unused) {
  if (!opts.target) return;
  MI_LOG(ACT_NDX_START);
  if (stat_timer) pinger_stop("by request"); else pinger_start();
  appbar_update();
}

static void on_pauseresume(GSimpleAction *action, GVariant *var, void *unused) {
  MI_LOG(ACT_NDX_PAUSE);
  pinger_state.pause = !pinger_state.pause;
  action_update();
  pinger_update_tabs(NULL);
  GRAPH_UPDATE_REL;
  if (OPTS_GRAPH_REL) { if (pinger_state.pause) series_lock(); else series_unlock(); }
}

static void on_reset(GSimpleAction *action, GVariant *var, void *unused) {
  MI_LOG(ACT_NDX_RESET);
  pinger_clear_data(false);
}

static void hide_help_cb(GtkWidget *button, GtkWidget *dialog) {
  if (GTK_IS_WIDGET(dialog)) gtk_widget_set_visible(dialog, false);
}

static gboolean help_dialog_cb(GtkEventControllerKey *c, unsigned val, unsigned code, unsigned mo, GtkButton *btn) {
  if ((val != GDK_KEY_Escape) || (mo & GDK_MODIFIER_MASK) || !GTK_IS_BUTTON(btn)) return false;
  g_signal_emit_by_name(btn, EV_CLICK);
  return true;
}

static gboolean show_help_dialog(GtkWidget *win) {
  if (!GTK_IS_WIDGET(win)) return false;
  MA_LOG(ACT_NDX_HELP); gtk_widget_set_visible(win, true); return true;
}

static void on_help(GSimpleAction *action, GVariant *var, void *data) {
  static t_help_dialog_in help_dialog;
  GtkWidget *win = data; g_return_if_fail(GTK_IS_WINDOW(win));
  if (show_help_dialog(help_dialog.win)) return;
  //
  if (!GTK_IS_WINDOW(help_dialog.win)) {
    help_dialog.win = gtk_window_new();
    g_return_if_fail(GTK_IS_WINDOW(help_dialog.win));
    gtk_window_set_hide_on_close(GTK_WINDOW(help_dialog.win), true);
    gtk_window_set_decorated(GTK_WINDOW(help_dialog.win), false);
    gtk_window_set_modal(GTK_WINDOW(help_dialog.win), true);
    gtk_window_set_transient_for(GTK_WINDOW(help_dialog.win), GTK_WINDOW(win));
    if (style_loaded) gtk_widget_add_css_class(help_dialog.win, CSS_ROUNDG);
  }
  //
  if (!GTK_IS_BOX(help_dialog.box)) {
    help_dialog.box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
    g_return_if_fail(GTK_IS_BOX(help_dialog.box));
    gtk_window_set_child(GTK_WINDOW(help_dialog.win), help_dialog.box);
  }
  //
  if (!GTK_IS_LABEL(help_dialog.hdr)) {
    help_dialog.hdr = gtk_label_new(appver);
    g_return_if_fail(GTK_IS_LABEL(help_dialog.hdr));
    if (style_loaded) gtk_widget_add_css_class(help_dialog.hdr, CSS_PAD6);
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.hdr);
    GtkWidget *div = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    if (GTK_IS_SEPARATOR(div)) gtk_box_append(GTK_BOX(help_dialog.box), div);
  }
  //
  if (!GTK_IS_SCROLLED_WINDOW(help_dialog.scroll)) {
    help_dialog.scroll = gtk_scrolled_window_new();
    g_return_if_fail(GTK_IS_SCROLLED_WINDOW(help_dialog.scroll));
    gtk_widget_set_size_request(help_dialog.scroll, X_RES * 0.47, Y_RES * 0.76);
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.scroll);
  }
  //
  if (!GTK_IS_BUTTON(help_dialog.btn)) {
    help_dialog.btn = gtk_button_new_with_label(OKAY_BUTTON);
    g_return_if_fail(GTK_IS_BUTTON(help_dialog.btn));
    g_signal_connect(help_dialog.btn, EV_CLICK, G_CALLBACK(hide_help_cb), help_dialog.win);
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.btn);
    GtkEventController *kcntrl = gtk_event_controller_key_new();
    if (GTK_IS_EVENT_CONTROLLER(kcntrl)) { // optional
      g_signal_connect(kcntrl, EV_KEY, G_CALLBACK(help_dialog_cb), help_dialog.btn);
      gtk_widget_add_controller(help_dialog.win, kcntrl);
    }
  }
  //
  if (!GTK_IS_LABEL(help_dialog.body)) {
    help_dialog.body = gtk_label_new(help_message);
    g_return_if_fail(GTK_IS_LABEL(help_dialog.body));
    gtk_label_set_use_markup(GTK_LABEL(help_dialog.body), true);
    gtk_label_set_selectable(GTK_LABEL(help_dialog.body), false);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(help_dialog.scroll), help_dialog.body);
  }
  //
  show_help_dialog(help_dialog.win);
}

static void on_quit(GSimpleAction *action, GVariant *var, void *data) {
  g_return_if_fail(GTK_IS_WINDOW(data));
  MA_LOG(ACT_NDX_QUIT);
  gtk_window_close(GTK_WINDOW(data));
}

static void on_legend(GSimpleAction *action, GVariant *var, void *unused) {
  MA_LOG(ACT_NDX_LGND);
  opts.legend = !opts.legend;
  option_legend();
}

static GMenu* action_menu_init(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), NULL);
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), NULL);
  gboolean okay = true;
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), button);
  const char *icons[] = {ACT_MENU_ICON, ACT_MENU_ICOA, NULL};
  const char *ico = is_sysicon(icons);
  if (!ico) WARN("No icon found for %s", "action menu");
  else gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), ico);
  gtk_widget_set_tooltip_text(button, ACT_TOOLTIP);
  GMenu *menu = g_menu_new();
  if (G_IS_MENU(menu)) {
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    if (GTK_IS_POPOVER_MENU(popover)) gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
    else okay = false;
  } else okay = false;
  if (!okay) gtk_header_bar_remove(GTK_HEADER_BAR(bar), button);
  return okay ? menu : NULL;
}

static gboolean create_action_menu(GtkApplication *app, GtkWidget *win, GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app), false);
  g_return_val_if_fail(GTK_IS_WINDOW(win), false);
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  for (int i = 0; i < ACT_NDX_MAX; i++) act_entries[i].name = act_desc[i].name + ACT_DOT;
  g_action_map_add_action_entries(G_ACTION_MAP(app), act_entries, ACT_NDX_MAX, win);
  for (int i = 0; i < ACT_NDX_MAX; i++)
    act_desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), act_entries[i].name));
  if (!(action_menu = action_menu_init(bar))) return false;
  for (int i = 0; i < ACT_NDX_MAX; i++)
    gtk_application_set_accels_for_action(app, act_desc[i].name, act_desc[i].shortcut);
  SET_SA(act_desc, ACT_NDX_LGND, opts.graph);
  return true;
}

static void action_update_internal(GMenu *menu) {
  if (G_IS_MENU(menu)) {
    g_menu_remove_all(menu);
    for (int i = 0; i < ACT_NDX_MAX; i++) if (!act_desc[i].invisible) {
      GMenuItem *item = g_menu_item_new(action_label(i), act_desc[i].name);
      if (item) { g_menu_append_item(menu, item); g_object_unref(item); }
      else FAIL("g_menu_item_new()");
    }
  }
  gboolean run = pinger_state.run, pause = pinger_state.pause;
  SET_SA(act_desc, ACT_NDX_START, opts.target != NULL);
  SET_SA(act_desc, ACT_NDX_PAUSE, pause || (!pause && run));
  SET_SA(act_desc, ACT_NDX_RESET, run);
}


// pub
//

gboolean action_init(GtkApplication *app, GtkWidget* win, GtkWidget* bar) {
  g_return_val_if_fail(create_action_menu(app, win, bar), false);
  return true;
}

void action_update(void) { action_update_internal(action_menu); }

