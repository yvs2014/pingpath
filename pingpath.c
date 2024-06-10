
#include <stdlib.h>
#include <sysexits.h>
#include <locale.h>

#include "common.h"
#include "cli.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "tabs/ping.h"
#include "tabs/log.h"
#include "draw_rel.h"

#define APPFLAGS G_APPLICATION_NON_UNIQUE

#define APPQUIT(fmt, ...) { WARN("init " fmt " failed", __VA_ARGS__); \
  g_application_quit(G_APPLICATION(app)); exit_code = EXIT_FAILURE; return; }

static int exit_code = EXIT_SUCCESS;
static GtkWidget *currtabref, *tabrefs[TAB_NDX_MAX];

static void on_win_destroy(GtkWidget *widget, void *unused) { atquit = true; }

static void on_tab_switch(GtkNotebook *nb, GtkWidget *tab, unsigned ndx, void *unused) {
  tab_dependent(tab);
  if (GTK_IS_BOX(tab)) for (GtkWidget *p = gtk_widget_get_first_child(tab); p; p = gtk_widget_get_next_sibling(p)) {
    if (GTK_IS_LIST_BOX(p)) gtk_list_box_unselect_all(GTK_LIST_BOX(p));
    else if (GTK_IS_SCROLLED_WINDOW(p)) {
      GtkWidget *c = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(p));
      if (GTK_IS_VIEWPORT(c)) c = gtk_viewport_get_child(GTK_VIEWPORT(c));
      if (GTK_IS_BOX(c)) for (GtkWidget *n = gtk_widget_get_first_child(c); n; n = gtk_widget_get_next_sibling(n))
        if (GTK_IS_LIST_BOX(n)) gtk_list_box_unselect_all(GTK_LIST_BOX(n));
    }
  }
}

static void app_cb(GtkApplication* app, void *unused) {
  STYLE_SET;
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
  if (style_loaded) gtk_widget_add_css_class(nb, CSS_BGROUND);
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_BOTTOM);
  nb_tabs[TAB_PING_NDX] = pingtab_init(win);
  if (opts.graph) nb_tabs[TAB_GRAPH_NDX] = graphtab_init(win);
#ifdef WITH_PLOT
  if (opts.plot) nb_tabs[TAB_PLOT_NDX] = plottab_init(win);
#endif
  nb_tabs[TAB_LOG_NDX]  = logtab_init(win);
  for (int i = 0; i < G_N_ELEMENTS(nb_tabs); i++) {
    { if (!opts.graph && (i == TAB_GRAPH_NDX)) continue;
#ifdef WITH_PLOT
      if (!opts.plot && (i == TAB_PLOT_NDX)) continue;
#endif
    } // TODO: replace with -1,-2,-3 options
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
  g_signal_connect(win, EV_DESTROY, G_CALLBACK(on_win_destroy), NULL);
  gtk_window_present(GTK_WINDOW(win));
  { char *ver; // log runtime versions
    LOG("%c%s: " VERSION, g_ascii_toupper(APPNAME[0]), &(APPNAME[1]));
    if ((ver = rtver(GTK_STRV)))   { LOG("GTK: %s", ver);   g_free(ver); }
    if ((ver = rtver(GLIB_STRV)))  { LOG("Glib: %s", ver);  g_free(ver); }
    if ((ver = rtver(CAIRO_STRV))) { LOG("Cairo: %s", ver); g_free(ver); }
    if ((ver = rtver(PANGO_STRV))) { LOG("Pango: %s", ver); g_free(ver); }
  }
  if (autostart && opts.target) { pinger_start(); appbar_update(); }
}

static void recap_cb(GApplication* app, void *unused) {
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

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");  // parser: early l10n for CLI options
  putenv("LANG=C.UTF-8"); // parser: disable ping's i18n output
#if GTK_CHECK_VERSION(4, 14, 0)
  if (!getenv("GSK_RENDERER")) putenv("GSK_RENDERER=gl"); // to avoid ngl-n-vulkan issues with 4.14
#endif
  g_return_val_if_fail(parser_init(), EX_SOFTWARE);
  { gboolean valid_cli_options = cli_init(&argc, &argv);
    g_return_val_if_fail(valid_cli_options, EX_USAGE); }
  stat_init(true);
  pinger_init();
  GApplication *app;
  if (opts.recap) {
    app = g_application_new("net.tools." APPNAME, APPFLAGS);
    g_return_val_if_fail(G_IS_APPLICATION(app), EX_UNAVAILABLE);
    g_signal_connect(app, EV_ACTIVE, G_CALLBACK(recap_cb), NULL);
  } else {
    GtkApplication *gtkapp = gtk_application_new("net.tools." APPNAME, APPFLAGS);
    g_return_val_if_fail(GTK_IS_APPLICATION(gtkapp), EX_UNAVAILABLE);
    g_signal_connect(gtkapp, EV_ACTIVE, G_CALLBACK(app_cb), NULL);
    app = G_APPLICATION(gtkapp);
  }
  int rc = g_application_run(app, argc, argv);
  pinger_cleanup();
  g_object_unref(app);
  return exit_code || rc;
}

