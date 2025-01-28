
#include <stdlib.h>
#include <sysexits.h>
#ifdef WITH_NLS
#include <libintl.h>
#endif

#include "common.h"
#include "cli.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "ui/clipboard.h"
#include "tabs/aux.h"
#include "tabs/ping.h"
#include "tabs/graph.h"
#ifdef WITH_PLOT
#include "tabs/plot.h"
#endif
#include "tabs/log.h"

#define APPFLAGS G_APPLICATION_NON_UNIQUE

#define APPQUIT(fmt, ...) { WARN("init " fmt " failed", __VA_ARGS__); \
  g_application_quit(G_APPLICATION(app)); exit_code = EXIT_FAILURE; return; }

static int exit_code = EXIT_SUCCESS;
static GtkWidget *currtabref, *tabrefs[TAB_NDX_MAX];
#ifdef WITH_PLOT
static GtkNotebook *nb_ref;
#endif

static gboolean on_win_close(GtkWindow *self G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
  // workarounds to exit gracefully
  cb_tab_unsel(nb_tabs[TAB_PING_NDX]);
  cb_tab_unsel(nb_tabs[TAB_LOG_NDX]);
#ifdef WITH_PLOT
  if (GTK_IS_NOTEBOOK(nb_ref))
    for (int i = gtk_notebook_get_n_pages(nb_ref) - 1; i >= 0; i--)
      gtk_notebook_remove_page(nb_ref, i);
#endif
  atquit = true;
  return false;
}

static void on_tab_switch(GtkNotebook *self G_GNUC_UNUSED, GtkWidget *tab,
    guint page_num G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  tab_dependent(tab);
  if (GTK_IS_BOX(tab))
    for (GtkWidget *widget = gtk_widget_get_first_child(tab);
         widget; widget = gtk_widget_get_next_sibling(widget)) {
    if (GTK_IS_LIST_BOX(widget)) gtk_list_box_unselect_all(GTK_LIST_BOX(widget));
    else if (GTK_IS_SCROLLED_WINDOW(widget)) {
      GtkWidget *obj = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(widget));
      if (GTK_IS_VIEWPORT(obj)) obj = gtk_viewport_get_child(GTK_VIEWPORT(obj));
      if (GTK_IS_BOX(obj))
        for (GtkWidget *box = gtk_widget_get_first_child(obj);
             box; box = gtk_widget_get_next_sibling(box))
          if (GTK_IS_LIST_BOX(box)) gtk_list_box_unselect_all(GTK_LIST_BOX(box));
    }
  }
}

static inline void log_libver(const char *pre, char *ver) {
  if (ver) { LOG("%s: %s", pre, ver); g_free(ver); }}

static void on_app_activate(GtkApplication* app, gpointer user_data G_GNUC_UNUSED) {
  style_set();
  GtkWidget *win = gtk_application_window_new(app);
  if (!GTK_IS_WINDOW(win)) APPQUIT("%s", "app window");
  gtk_window_set_default_size(GTK_WINDOW(win), X_RES, Y_RES);
  if (style_loaded) gtk_widget_add_css_class(win, CSS_BGROUND);
  { const char *icons[] = {APPNAME, NULL};
    const char *ico = is_sysicon(icons);
    if (ico) gtk_window_set_icon_name(GTK_WINDOW(win), ico);
    else VERBOSE("no '%s' icon", APPNAME);
  }
  if (!appbar_init(app, win)) APPQUIT("%s", "appbar");
  GtkWidget *nb = gtk_notebook_new();
  if (!GTK_IS_NOTEBOOK(nb)) APPQUIT("%s", "notebook");
#ifdef WITH_PLOT
  nb_ref = GTK_NOTEBOOK(nb);
#endif
  if (style_loaded) gtk_widget_add_css_class(nb, CSS_BGROUND);
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_BOTTOM);
  nb_tabs[TAB_PING_NDX] = pingtab_init(win);
  if (opts.graph) nb_tabs[TAB_GRAPH_NDX] = graphtab_init();
#ifdef WITH_PLOT
  if (opts.plot) nb_tabs[TAB_PLOT_NDX] = plottab_init();
#endif
  nb_tabs[TAB_LOG_NDX]  = logtab_init(win);
  for (unsigned i = 0; i < G_N_ELEMENTS(nb_tabs); i++) {
    { if (!opts.graph && (i == TAB_GRAPH_NDX)) continue;
#ifdef WITH_PLOT
      if (!opts.plot  && (i == TAB_PLOT_NDX))  continue;
#endif
    }
    t_tab *tab = nb_tabs[i];
    if (!tab || !tab->tab.w || !tab->lab.w) APPQUIT("tab#%d", i);
    tab_setup(tab); tabrefs[i] = tab->tab.w;
    int ndx = gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab->tab.w, tab->lab.w);
    if (ndx < 0) APPQUIT("tab page#%d", i);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(nb), tab->tab.w, true);
  }
  tab_reload_theme();
  gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), activetab);
  g_signal_connect(nb, EV_TAB_SWITCH, G_CALLBACK(on_tab_switch), NULL);
  GtkWidget *over = notifier_init(NT_MAIN_NDX, nb);
  if (!GTK_IS_OVERLAY(over)) APPQUIT("%s", "notification window");
  GtkWidget *curr = gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb), gtk_notebook_get_current_page(GTK_NOTEBOOK(nb)));
  tab_dependent(curr ? curr : nb_tabs[activetab]->tab.w);
  gtk_window_set_child(GTK_WINDOW(win), over);
  //
  g_signal_connect(win, EV_CLOSEQ, G_CALLBACK(on_win_close), NULL);
  gtk_window_present(GTK_WINDOW(win));
  // log runtime versions
  LOG("%c%s: " VERSION, g_ascii_toupper(APPNAME[0]), &APPNAME[1]);
  log_libver("GTK",   rtver(GTK_STRV));
  log_libver("Glib",  rtver(GLIB_STRV));
  log_libver("Cairo", rtver(CAIRO_STRV));
  log_libver("Pango", rtver(PANGO_STRV));
  if (autostart && opts.target) { pinger_start(); appbar_update(); }
}

static void on_recap_activate(GApplication *app, gpointer user_data G_GNUC_UNUSED) {
  g_timeout_add_seconds(1, (GSourceFunc)pinger_recap_cb, app);
  if (G_IS_APPLICATION(app)) g_application_hold(app);
  pinger_start();
}


// pub

void tab_dependent(GtkWidget *tab) {
  if (tab) currtabref = tab; else tab = currtabref;
  if (tab == tabrefs[TAB_GRAPH_NDX]) nt_dark = !opts.darkgraph;
#ifdef WITH_PLOT
  else if (tab == tabrefs[TAB_PLOT_NDX]) nt_dark = !opts.darkplot;
#endif
  else nt_dark = !opts.darktheme;
}

#ifdef WITH_PLOT
inline gboolean is_tab_that(unsigned ndx) { return (ndx < G_N_ELEMENTS(tabrefs)) ? (currtabref == tabrefs[ndx]) : false; }
#endif

#ifdef HAVE_SECURE_GETENV
#define GETENV secure_getenv
#else
#define GETENV getenv
#endif

#ifdef WITH_NLS
#define SETLOCALE setlocale(LC_ALL, "")
#else
#define SETLOCALE setlocale(LC_ALL, "C")
#endif

#ifdef HAVE_USELOCALE
locale_t locale, localeC;
#ifdef WITH_NLS
#define USELOCALE uselocale(locale)
#else
#define USELOCALE uselocale(localeC)
#endif
#endif

int main(int argc, char **argv) {
  // init locales
#ifdef HAVE_USELOCALE
  locale  = newlocale(LC_ALL_MASK, "",  NULL);
  localeC = newlocale(LC_ALL_MASK, "C", NULL);
  if (locale && localeC)
    USELOCALE;
  else {
    if (locale)  { freelocale(locale);  locale  = 0; }
    if (localeC) { freelocale(localeC); localeC = 0; }
    SETLOCALE;
  }
#else
  SETLOCALE;
#endif
#ifdef WITH_NLS
  // NLS bindings
  bindtextdomain(APPNAME, LOCALEDIR);
  bind_textdomain_codeset(APPNAME, "UTF-8");
  textdomain(APPNAME);
#endif
  // GSK workarounds
  if (!GETENV("GSK_RENDERER")) {
    unsigned sure = gtk_get_major_version(), fix = gtk_get_minor_version();
    if (sure == 4) switch (fix) {
      case 14: // renderer workarounds since 4.14
      case 15: putenv("GSK_RENDERER=gl");     break;
      case 16: putenv("GSK_RENDERER=ngl");    break;
//    case 17: putenv("GSK_RENDERER=vulkan"); break; /* not needed yet */
      default: break;
    }
  }
  //
  if (!parser_init()) return EX_SOFTWARE;
  { int quit = cli_init(&argc, &argv);
    if (quit) return (quit > 0) ? EX_USAGE : EXIT_SUCCESS; }
  stat_init(true);
  pinger_init();
  GApplication *app = NULL;
  if (opts.recap) {
    app = g_application_new("net.tools." APPNAME, APPFLAGS);
    g_return_val_if_fail(G_IS_APPLICATION(app), EX_UNAVAILABLE);
    g_signal_connect(app, EV_ACTIVE, G_CALLBACK(on_recap_activate), NULL);
  } else {
    GtkApplication *gtkapp = gtk_application_new("net.tools." APPNAME, APPFLAGS);
    g_return_val_if_fail(GTK_IS_APPLICATION(gtkapp), EX_UNAVAILABLE);
    g_signal_connect(gtkapp, EV_ACTIVE, G_CALLBACK(on_app_activate), NULL);
    app = G_APPLICATION(gtkapp);
  }
  int rc = g_application_run(app, argc, argv);
  pinger_cleanup();
  g_object_unref(app);
#ifdef HAVE_USELOCALE
  if (locale)  freelocale(locale);
  if (localeC) freelocale(localeC);
#else
  setlocale(LC_ALL, NULL);
#endif
  return exit_code || rc;
}

