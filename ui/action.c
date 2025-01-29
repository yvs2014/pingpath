
#include <stdlib.h>

#include "text.h"
#include "common.h"
#include "action.h"
#include "appbar.h"
#include "option.h"
#include "notifier.h"
#include "pinger.h"
#include "series.h"
#include "ui/style.h"
#include "tabs/aux.h"
#ifdef WITH_PLOT
#include "tabs/plot.h"
#endif

#if PANGO_VERSION_MAJOR == 1
#if PANGO_VERSION_MINOR < 50
#define LINE_HEIGHT(f) ""
#else
#define LINE_HEIGHT(f) "line_height='" f "'"
#endif
#endif

#define MA_LOG(ndx, lab) LOG("Action %s", lab(ndx))
#define MI_LOG(ndx, lab) notifier_inform("Action %s", lab(ndx))

typedef struct help_dialog_in {
  GtkWidget *win, *box, *hdr, *scroll, *body, *btn;
} t_help_dialog_in;

typedef struct help_line_s {
  char *l, *r;
} help_line_s;

typedef struct help_section_s {
  char *name;
  help_line_s *lines;
  unsigned len;
} help_section_s;

enum { MENU_SA_START, MENU_SA_PAUSE, MENU_SA_RESET, MENU_SA_HELP, MENU_SA_QUIT, MENU_SA_MAX };

static const char* kb_space[]      = {"space",   NULL};
static const char* kb_ctrl_h[]     = {"<Ctrl>h", NULL};
static const char* kb_ctrl_l[]     = {"<Ctrl>l", NULL};
static const char* kb_ctrl_r[]     = {"<Ctrl>r", NULL};
static const char* kb_ctrl_s[]     = {"<Ctrl>s", NULL};
static const char* kb_ctrl_x[]     = {"<Ctrl>x", NULL};
#ifdef WITH_PLOT
static const char* kb_ctrl_left[]  = {"<Ctrl>Left",      "KP_Left",      NULL};
static const char* kb_ctrl_right[] = {"<Ctrl>Right",     "KP_Right",     NULL};
static const char* kb_ctrl_up[]    = {"<Ctrl>Up",        "KP_Up",        NULL};
static const char* kb_ctrl_down[]  = {"<Ctrl>Down",      "KP_Down",      NULL};
static const char* kb_ctrl_pgup[]  = {"<Ctrl>Page_Up",   "KP_Page_Up", "KP_Page_Down", "KP_Begin", NULL};
static const char* kb_ctrl_pgdn[]  = {"<Ctrl>Page_Down", "KP_Home",    "KP_End", NULL};
static const char* kb_ctrl_in[]    = {"<Ctrl>End",       "KP_Add",       NULL};
static const char* kb_ctrl_out[]   = {"<Ctrl>Home",      "KP_Subtract",  NULL};
#endif

#ifdef WITH_PLOT
#define LGL_SET(labndx, lsign, gsign) { .label  = (labndx), \
  .global = { .sign = (gsign), .rev = false }, \
  .local  = { .sign = (lsign), .rev = (lsign) != (gsign) }}
t_kb_plot_aux kb_plot_aux[ACCL_SA_MAX] = {
  [ACCL_SA_LEFT]  = LGL_SET(ACCL_SA_LEFT,   1, -1),
  [ACCL_SA_RIGHT] = LGL_SET(ACCL_SA_RIGHT, -1,  1),
  [ACCL_SA_UP]    = LGL_SET(ACCL_SA_UP,    -1, -1),
  [ACCL_SA_DOWN]  = LGL_SET(ACCL_SA_DOWN,   1,  1),
  [ACCL_SA_PGUP]  = LGL_SET(ACCL_SA_PGUP,   1,  1),
  [ACCL_SA_PGDN]  = LGL_SET(ACCL_SA_PGDN,  -1, -1),
  [ACCL_SA_IN]    = { .label = ACCL_SA_IN  },
  [ACCL_SA_OUT]   = { .label = ACCL_SA_OUT },
};
#undef LGL_SET
#endif

static t_sa_desc menu_sa_desc[MENU_SA_MAX] = {
  [MENU_SA_START] = { .name = "app.act_start", .shortcut = kb_ctrl_s },
  [MENU_SA_PAUSE] = { .name = "app.act_pause", .shortcut = kb_space  },
  [MENU_SA_RESET] = { .name = "app.act_reset", .shortcut = kb_ctrl_r },
  [MENU_SA_HELP]  = { .name = "app.act_help",  .shortcut = kb_ctrl_h },
  [MENU_SA_QUIT]  = { .name = "app.act_exit",  .shortcut = kb_ctrl_x },
};

static t_sa_desc accl_sa_desc[] = {
  [ACCL_SA_LGND]  = { .name = "app.act_lgnd",  .shortcut = kb_ctrl_l },
#ifdef WITH_PLOT
  [ACCL_SA_LEFT]  = { .name = "app.act_left",  .shortcut = kb_ctrl_left,  .data = &kb_plot_aux[ACCL_SA_LEFT]  },
  [ACCL_SA_RIGHT] = { .name = "app.act_right", .shortcut = kb_ctrl_right, .data = &kb_plot_aux[ACCL_SA_RIGHT] },
  [ACCL_SA_UP]    = { .name = "app.act_up",    .shortcut = kb_ctrl_up,    .data = &kb_plot_aux[ACCL_SA_UP]    },
  [ACCL_SA_DOWN]  = { .name = "app.act_down",  .shortcut = kb_ctrl_down,  .data = &kb_plot_aux[ACCL_SA_DOWN]  },
  [ACCL_SA_PGUP]  = { .name = "app.act_pgup",  .shortcut = kb_ctrl_pgup,  .data = &kb_plot_aux[ACCL_SA_PGUP]  },
  [ACCL_SA_PGDN]  = { .name = "app.act_pgdn",  .shortcut = kb_ctrl_pgdn,  .data = &kb_plot_aux[ACCL_SA_PGDN]  },
  [ACCL_SA_IN]    = { .name = "app.act_in",    .shortcut = kb_ctrl_in,    .data = &kb_plot_aux[ACCL_SA_IN]    },
  [ACCL_SA_OUT]   = { .name = "app.act_out",   .shortcut = kb_ctrl_out,   .data = &kb_plot_aux[ACCL_SA_OUT]   },
#endif
};

static void on_startstop  (GSimpleAction*, GVariant*, void*);
static void on_pauseresume(GSimpleAction*, GVariant*, void*);
static void on_reset      (GSimpleAction*, GVariant*, void*);
static void on_help       (GSimpleAction*, GVariant*, GtkWindow*);
static void on_quit       (GSimpleAction*, GVariant*, GtkWindow*);
static void on_legend     (GSimpleAction*, GVariant*, void*);
#ifdef WITH_PLOT
static void on_scale      (GSimpleAction*, GVariant*, t_kb_plot_aux*);
#endif
typedef void (*ae_sa_fn)  (GSimpleAction*, GVariant*, void*);

static GActionEntry menu_sa_entries[MENU_SA_MAX] = {
  [MENU_SA_START] = { .activate = on_startstop      },
  [MENU_SA_PAUSE] = { .activate = on_pauseresume    },
  [MENU_SA_RESET] = { .activate = on_reset          },
  [MENU_SA_HELP]  = { .activate = (ae_sa_fn)on_help },
  [MENU_SA_QUIT]  = { .activate = (ae_sa_fn)on_quit },
};

static GActionEntry accl_sa_entries[ACCL_SA_MAX] = {
  [ACCL_SA_LGND]  = { .activate = on_legend },
#ifdef WITH_PLOT
  [ACCL_SA_LEFT]  = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_RIGHT] = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_UP]    = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_DOWN]  = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_PGUP]  = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_PGDN]  = { .activate = (ae_sa_fn)on_rotation },
  [ACCL_SA_IN]    = { .activate = (ae_sa_fn)on_scale },
  [ACCL_SA_OUT]   = { .activate = (ae_sa_fn)on_scale },
#endif
};

static GMenu *action_menu;

static const char* menu_sa_label(int ndx) {
  switch (ndx) {
    case MENU_SA_START:
      return pinger_state.run   ? ACT_STOP_HDR   : ACT_START_HDR;
    case MENU_SA_PAUSE:
      return pinger_state.pause ? ACT_RESUME_HDR : ACT_PAUSE_HDR;
    case MENU_SA_RESET: return ACT_RESET_HDR;
    case MENU_SA_HELP:  return ACT_HELP_HDR;
    case MENU_SA_QUIT:  return ACT_QUIT_HDR;
    default: break;
  }
  return "";
}

static const char* accl_sa_label(int ndx) {
  switch (ndx) {
    case ACCL_SA_LGND:  return ACT_LGND_HDR;
#ifdef WITH_PLOT
    case ACCL_SA_LEFT:  return ACT_LEFT_K_HDR;
    case ACCL_SA_RIGHT: return ACT_RIGHT_K_HDR;
    case ACCL_SA_UP:    return ACT_UP_K_HDR;
    case ACCL_SA_DOWN:  return ACT_DOWN_K_HDR;
    case ACCL_SA_PGUP:  return ACT_PGUP_K_HDR;
    case ACCL_SA_PGDN:  return ACT_PGDN_K_HDR;
    case ACCL_SA_IN:    return ACT_IN_K_HDR;
    case ACCL_SA_OUT:   return ACT_OUT_K_HDR;
#endif
    default: break;
  }
  return "";
}

static void on_startstop(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  if (!opts.target) return;
  MI_LOG(MENU_SA_START, menu_sa_label);
  if (stat_timer) pinger_stop("by request"); else pinger_start();
  appbar_update();
}

static void on_pauseresume(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  MI_LOG(MENU_SA_PAUSE, menu_sa_label);
  pinger_state.pause = !pinger_state.pause;
  action_update();
  pinger_update_tabs(NULL);
  if (need_drawing()) {
    drawtab_update();
    if (pinger_state.pause) series_lock(); else series_unlock();
  }
}

static void on_reset(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  MI_LOG(MENU_SA_RESET, menu_sa_label);
  pinger_clear_data(false);
}

static void on_hide_help(GtkButton *self G_GNUC_UNUSED, GtkWidget *dialog) {
  if (GTK_IS_WIDGET(dialog)) gtk_widget_set_visible(dialog, false);
}

static void help_on_escape(GtkEventControllerKey *self G_GNUC_UNUSED,
    guint keyval, guint keycode G_GNUC_UNUSED, GdkModifierType state, GtkButton *button)
{
  if ((keyval == GDK_KEY_Escape) && !(state & GDK_MODIFIER_MASK) && GTK_IS_BUTTON(button))
    g_signal_emit_by_name(button, EV_CLICK);
}

static gboolean show_help_dialog(GtkWidget *win) {
  if (!GTK_IS_WIDGET(win)) return false;
  MA_LOG(MENU_SA_HELP, menu_sa_label);
  gtk_widget_set_visible(win, true);
  return true;
}

static inline GtkWidget* get_help_content(void) {
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), NULL);
  //
  help_line_s actions[] = {
    { .l = g_strdup_printf("%s/%s", ACT_START_HDR, ACT_STOP_HDR),
      .r = g_strdup(_("pinging")) },
    { .l = g_strdup_printf("%s/%s", ACT_PAUSE_HDR, ACT_RESUME_HDR),
      .r = g_strdup(_("data refreshing")) },
    { .l = g_strdup(ACT_RESET_HDR),
      .r = g_strdup(_("statistics data")) },
    { .l = g_strdup(ACT_QUIT_HDR),
      .r = g_strdup(_("stop and quit")) },
  };
  help_line_s main_options[] = {
    { .l = g_strdup(OPT_CYCLES_HDR),
      .r = g_strdup_printf("%s [%d]", _("Number of ping cycles"), DEF_CYCLES) },
    { .l = g_strdup(OPT_IVAL_HDR),
      .r = g_strdup_printf("%s [%d]", _("Gap in seconds between pings"), DEF_TOUT) },
    { .l = g_strdup(OPT_DNS_HDR),
      .r = g_strdup(_("IP address resolving, on | off")) },
    { .l = g_strdup(OPT_INFO_HDR),
      .r = g_strdup_printf("%s:\n%s, %s, %s, %s, %s",
            _("Hop elements to display"),
             ELEM_HOST_HDR, ELEM_AS_HDR, ELEM_CC_HDR, ELEM_DESC_HDR, ELEM_RT_HDR) },
    { .l = g_strdup(OPT_STAT_HDR),
      .r = g_strdup_printf("%s:\n%s, %s, %s, %s, %s, %s, %s, %s",
             _("Stat elements to display"),
             ELEM_LOSS_HDR, ELEM_SENT_HDR, ELEM_RECV_HDR, ELEM_LAST_HDR,
             ELEM_BEST_HDR, ELEM_WRST_HDR, ELEM_AVRG_HDR, ELEM_JTTR_HDR) },
    { .l = g_strdup(OPT_TTL_HDR),
      .r = g_strdup_printf("%s [%d - %d]", _("TTL range"), 0, MAXTTL) },
    { .l = g_strdup(OPT_QOS_HDR),
      .r = g_strdup_printf("%s [%d]", _("QoS/ToS byte"), DEF_QOS) },
    { .l = g_strdup(OPT_PLOAD_HDR),
      .r = g_strdup_printf("%s [%s]", _("Up to 16 bytes in hex format"), DEF_PPAD) },
    { .l = g_strdup(OPT_PSIZE_HDR),
      .r = g_strdup_printf("%s [%d]", _("ICMP data size"), DEF_PSIZE) },
    { .l = g_strdup(OPT_IPV_HDR),
      .r = g_strdup_printf("%s | %s | %s", OPT_IPVA_HDR, OPT_IPV4_HDR, OPT_IPV6_HDR) },
  };
  help_line_s aux_options[] = {
    { .l = g_strdup(HELP_THEME_MAIN),
      .r = g_strdup_printf("%s | %s", _("dark"), _("light")) },
    { .l = g_strdup(HELP_THEME_GRAPH),
      .r = g_strdup_printf("%s | %s", _("light"), _("dark")) },
#ifdef WITH_PLOT
    { .l = g_strdup(HELP_THEME_3D),
      .r = g_strdup_printf("%s | %s", _("light"), _("dark")) },
#endif
    { .l = g_strdup(OPT_GRAPH_HDR),
      .r = g_strdup_printf("%s | %s | %s",
             OPT_GR_DOT_HDR, OPT_GR_LINE_HDR, OPT_GR_CURVE_HDR) },
    { .l = g_strdup(OPT_GRLG_HDR),
      .r = g_strdup_printf("%s, %s, %s",
             GRLG_AVJT_HDR, GRLG_CCAS_HDR, GRLG_LGHN_HDR) },
    { .l = g_strdup(OPT_GREX_HDR),
      .r = g_strdup_printf("%s, %s, %s",
             GREX_MEAN_HDR, GREX_JRNG_HDR, GREX_AREA_HDR) },
#ifdef WITH_PLOT
    { .l = g_strdup(OPT_PLOT_HDR),
      .r = g_strdup_printf("%s, %s, %s, %s",
             PLEL_BACK_HDR, PLEL_AXIS_HDR, PLEL_GRID_HDR, PLEL_ROTR_HDR) },
    { .l = g_strdup(OPT_GRAD_HDR),
      .r = g_strdup_printf("%s:\n%s %s", _("3D-surface colors"),
             _("RGB color pairs like"), "r=80:80,g=230:80,b=80:230") },
    { .l = g_strdup(OPT_ROTOR_HDR),
      .r = g_strdup(_("Space, Orientation, and Step")) },
    { .l = g_strdup(OPT_SCALE_HDR),
      .r = g_strdup(OPT_FOV_HDR) },
#endif
    { .l = g_strdup(OPT_LOGMAX_HDR),
      .r = g_strdup_printf("%s [%d]", _("Max rows in log-tab"), DEF_LOGMAX) },
  };
  help_line_s cli_options[] = {
    { .l = g_strdup(_("for command-line options see " APPNAME "(1) manual page")) }
  };
  //
  help_section_s sections[] = {
    { .name  = g_strdup(HELP_ACT_TITLE),
      .lines = actions,
      .len   = G_N_ELEMENTS(actions) },
    { .name  = g_strdup(OPT_MAINMENU_TIP),
      .lines = main_options,
      .len   = G_N_ELEMENTS(main_options) },
    { .name  = g_strdup(OPT_AUXMENU_TIP),
      .lines = aux_options,
      .len   = G_N_ELEMENTS(aux_options) },
    { .name  = g_strdup(HELP_CLI_TITLE),
      .lines = cli_options,
      .len   = G_N_ELEMENTS(cli_options) },
  };
  //
#define ADD_H_LABEL(group, str, css) if (str) {        \
  GtkWidget *label = gtk_label_new(str);               \
  if (GTK_IS_LABEL(label)) {                           \
    gtk_label_set_selectable(GTK_LABEL(label), false); \
    gtk_box_append(GTK_BOX(box), label);               \
    gtk_label_set_xalign(GTK_LABEL(label), 0);         \
    gtk_widget_set_valign(label, GTK_ALIGN_START);     \
    gtk_widget_set_hexpand(label, true);               \
    if (style_loaded && (css))                         \
      gtk_widget_add_css_class(label, (css));          \
    if (group)                                         \
      gtk_size_group_add_widget((group), label);       \
  }                                                    \
}
  GtkSizeGroup *g0 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  GtkSizeGroup *g1 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  for (unsigned i = 0; i < G_N_ELEMENTS(sections); i++) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    if (GTK_IS_BOX(box)) {
      ADD_H_LABEL(0, sections[i].name, NULL);
      if (style_loaded) {
        gtk_widget_add_css_class(box, CSS_PAD4);
        gtk_widget_add_css_class(box, CSS_TPAD);
      }
      GtkWidget *div = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
      if (div) gtk_list_box_append(GTK_LIST_BOX(list), div);
      gtk_list_box_append(GTK_LIST_BOX(list), box);
    }
    g_free(sections[i].name);
    for (unsigned j = 0; j < sections[i].len; j++) {
      char *l = sections[i].lines[j].l;
      char *r = sections[i].lines[j].r;
      GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN * 2);
      if (GTK_IS_BOX(box)) {
        ADD_H_LABEL(r ? g0 : 0, l, CSS_LPAD_BOLD);
        ADD_H_LABEL(g1, r, CSS_RPAD);
        if (style_loaded)
          gtk_widget_add_css_class(box, CSS_PAD4);
        gtk_list_box_append(GTK_LIST_BOX(list), box);
      }
      g_free(l);
      g_free(r);
    }
  }
  g_object_unref(g0);
  g_object_unref(g1);
#undef ADD_H_LABEL
  //
  if (GTK_IS_LIST_BOX(list)) {
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_list_box_set_show_separators(GTK_LIST_BOX(list), false);
  }
  return list;
}

static void on_help(GSimpleAction *action G_GNUC_UNUSED, GVariant *parameter G_GNUC_UNUSED, GtkWindow *win) {
  static t_help_dialog_in help_dialog;
  if (!GTK_IS_WINDOW(win)) return;
  if (show_help_dialog(help_dialog.win)) return;
  //
  if (!GTK_IS_WINDOW(help_dialog.win)) {
    help_dialog.win = gtk_window_new();
    g_return_if_fail(GTK_IS_WINDOW(help_dialog.win));
    gtk_window_set_hide_on_close(GTK_WINDOW(help_dialog.win), true);
    gtk_window_set_decorated(GTK_WINDOW(help_dialog.win), false);
    gtk_window_set_modal(GTK_WINDOW(help_dialog.win), true);
    gtk_window_set_transient_for(GTK_WINDOW(help_dialog.win), win);
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
    help_dialog.hdr = gtk_label_new(APPVER);
    g_return_if_fail(GTK_IS_LABEL(help_dialog.hdr));
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.hdr);
  }
  //
  if (!GTK_IS_SCROLLED_WINDOW(help_dialog.scroll)) {
    help_dialog.scroll = gtk_scrolled_window_new();
    g_return_if_fail(GTK_IS_SCROLLED_WINDOW(help_dialog.scroll));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(help_dialog.scroll),
      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(help_dialog.scroll, -1, Y_RES * 0.8);
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.scroll);
  }
  //
  if (!GTK_IS_BUTTON(help_dialog.btn)) {
    help_dialog.btn = gtk_button_new_with_label(BUTTON_OKAY);
    g_return_if_fail(GTK_IS_BUTTON(help_dialog.btn));
//    gtk_button_set_use_underline(GTK_BUTTON(help_dialog.btn), true);
    g_signal_connect(help_dialog.btn, EV_CLICK, G_CALLBACK(on_hide_help), help_dialog.win);
    gtk_box_append(GTK_BOX(help_dialog.box), help_dialog.btn);
    GtkEventController *kcntrl = gtk_event_controller_key_new();
    if (GTK_IS_EVENT_CONTROLLER(kcntrl)) { // optional
      g_signal_connect(kcntrl, EV_KEY, G_CALLBACK(help_on_escape), help_dialog.btn);
      gtk_widget_add_controller(help_dialog.win, kcntrl);
    }
  }
  //
  if (!GTK_IS_WIDGET(help_dialog.body)) {
    help_dialog.body = get_help_content();
    g_return_if_fail(GTK_IS_WIDGET(help_dialog.body));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(help_dialog.scroll), help_dialog.body);
  }
  //
  show_help_dialog(help_dialog.win);
  if (help_dialog.btn)
    gtk_widget_grab_focus(help_dialog.btn);
}

static void on_quit(GSimpleAction *action G_GNUC_UNUSED, GVariant *parameter G_GNUC_UNUSED, GtkWindow *win) {
  if (GTK_IS_WINDOW(win)) { MA_LOG(MENU_SA_QUIT, menu_sa_label); gtk_window_close(win); }
}

static void on_legend(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
  MA_LOG(ACCL_SA_LGND, accl_sa_label);
  opts.legend = !opts.legend;
  option_legend();
}

#ifdef WITH_PLOT
void on_rotation(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, t_kb_plot_aux *data)
{
  if (!data || !is_tab_that(TAB_PLOT_NDX)) return;
  t_lg_space lg = opts.rglob ? data->global : data->local;
  if (!lg.aux || !lg.pval) return;
  int step = lg.sign * (data->step ? *data->step : 1);
  int want = *lg.pval + step;
  t_minmax *mm = lg.aux->mm ? lg.aux->mm : &opt_mm_rot;
  if (want < mm->min) want += lg.aux->wrap;
  if (want > mm->max) want -= lg.aux->wrap;
  if (*lg.pval == want) return;
  MA_LOG(data->label, accl_sa_label);
  notifier_inform("%s: %d", lg.aux->sfx, want);
  *lg.pval = want; option_up_menu_ext();
  set_rotor_n_redraw(step, lg.rev, lg.aux->pn);
}

void on_scale(GSimpleAction *action G_GNUC_UNUSED,
    GVariant *parameter G_GNUC_UNUSED, t_kb_plot_aux *data)
{
  if (!data || !is_tab_that(TAB_PLOT_NDX)) return;
  int step = (data->label == ACCL_SA_IN) ? -1 : ((data->label == ACCL_SA_OUT) ? 1 : 0);
  const t_ent_spn_aux *aux = data->global.aux;
  int *pval = aux->pval;
  if (!pval || !step) return;
  t_minmax *mm = aux->mm ? aux->mm : &opt_mm_fov;
  int want = *pval + step;
  if ((want < mm->min) || (want > mm->max)) return;
  *pval = want;
  notifier_inform("%s (%s): %d°", aux->prfx, aux->sfx, want);
  plottab_refresh(PL_PARAM_FOV);
}
#endif

static GMenu* action_menu_init(GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), NULL);
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), NULL);
  gboolean okay = true;
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), button);
  const char *icons[] = {ACT_MENU_ICON, ACT_MENU_ICOA, NULL};
  const char *ico = is_sysicon(icons);
  if (ico) gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), ico);
  else WARN("No icon found for %s", "action menu");
  gtk_widget_set_tooltip_text(button, OPT_ACTIONS_TIP);
  GMenu *menu = g_menu_new();
  if (G_IS_MENU(menu)) {
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    if (GTK_IS_POPOVER_MENU(popover))
      gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
    else okay = false;
  } else okay = false;
  if (!okay)
    gtk_header_bar_remove(GTK_HEADER_BAR(bar), button);
  return okay ? menu : NULL;
}

static void map_sa_entries(GActionMap *map, GActionEntry entr[], t_sa_desc desc[], int n, void *data) {
  for (int i = 0; i < n; i++) entr[i].name = desc[i].name + ACT_DOT;
  g_action_map_add_action_entries(map, entr, n, data);
  for (int i = 0; i < n; i++) desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(map, entr[i].name));
}

#define LOOP_SET_ACCELS(desc) { for (unsigned i = 0; i < G_N_ELEMENTS(desc); i++) \
  gtk_application_set_accels_for_action(app, (desc)[i].name, (desc)[i].shortcut); }

static gboolean create_action_menu(GtkApplication *app, GtkWidget *win, GtkWidget *bar) {
  g_return_val_if_fail(GTK_IS_APPLICATION(app) && GTK_IS_WINDOW(win) && GTK_IS_HEADER_BAR(bar), false);
  GActionMap *map = G_ACTION_MAP(app);
  map_sa_entries(map, menu_sa_entries, menu_sa_desc, G_N_ELEMENTS(menu_sa_entries), win);
  for (unsigned i = 0; i < G_N_ELEMENTS(accl_sa_entries); i++)
    map_sa_entries(map, &accl_sa_entries[i], &accl_sa_desc[i], 1, accl_sa_desc[i].data);
  if (!(action_menu = action_menu_init(bar))) return false;
  LOOP_SET_ACCELS(menu_sa_desc);
  LOOP_SET_ACCELS(accl_sa_desc);
  SET_SA(accl_sa_desc, ACCL_SA_LGND, opts.graph);
#ifdef WITH_PLOT
  SET_SA(accl_sa_desc, ACCL_SA_LEFT,  opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_RIGHT, opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_UP,    opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_DOWN,  opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_PGUP,  opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_PGDN,  opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_IN,    opts.plot);
  SET_SA(accl_sa_desc, ACCL_SA_OUT,   opts.plot);
#endif
  return true;
}

static void action_update_internal(GMenu *menu) {
  if (G_IS_MENU(menu)) {
    g_menu_remove_all(menu);
    for (unsigned i = 0; i < G_N_ELEMENTS(menu_sa_desc); i++) {
      GMenuItem *item = g_menu_item_new(menu_sa_label(i), menu_sa_desc[i].name);
      if (item) { g_menu_append_item(menu, item); g_object_unref(item); }}}
  gboolean run = pinger_state.run, pause = pinger_state.pause;
  SET_SA(menu_sa_desc, MENU_SA_START, opts.target != NULL);
  SET_SA(menu_sa_desc, MENU_SA_PAUSE, pause || (!pause && run));
  SET_SA(menu_sa_desc, MENU_SA_RESET, run);
}

#ifdef WITH_PLOT
#define APS_LINK(nth, rot, axis) { \
  kb_plot_aux[nth].local.aux   = &ent_spn[ENT_SPN_LOCAL].list[0].aux[rot]; \
  kb_plot_aux[nth].local.pval  = &opts.orient[axis]; \
  kb_plot_aux[nth].global.aux  = &ent_spn[ENT_SPN_GLOBAL].list[0].aux[axis]; \
  kb_plot_aux[nth].global.pval = &opts.orient[axis]; \
  kb_plot_aux[nth].step = &opts.angstep; \
}
static inline void kb_link_to_ent(void) {
  APS_LINK(ACCL_SA_LEFT,  0, 1);
  APS_LINK(ACCL_SA_RIGHT, 0, 1);
  APS_LINK(ACCL_SA_UP,    2, 0);
  APS_LINK(ACCL_SA_DOWN,  2, 0);
  APS_LINK(ACCL_SA_PGUP,  1, 2);
  APS_LINK(ACCL_SA_PGDN,  1, 2);
  kb_plot_aux[ACCL_SA_IN].global.aux  = ent_spn[ENT_SPN_FOV].list[0].aux;
  kb_plot_aux[ACCL_SA_IN].global.pval = &opts.fov;
  kb_plot_aux[ACCL_SA_OUT].global.aux  = kb_plot_aux[ACCL_SA_IN].global.aux;
  kb_plot_aux[ACCL_SA_OUT].global.pval = kb_plot_aux[ACCL_SA_IN].global.pval;
}
#undef KB_APS_LINK
#endif


// pub
//

gboolean action_init(GtkApplication *app, GtkWidget* win, GtkWidget* bar) {
#ifdef WITH_PLOT
  kb_link_to_ent();
#endif
  g_return_val_if_fail(create_action_menu(app, win, bar), false);
  return true;
}

void action_update(void) { action_update_internal(action_menu); }

