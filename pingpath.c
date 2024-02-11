
#include <stdlib.h>
#include <sysexits.h>
#include <locale.h>
#ifdef FCFINI
#include <fontconfig/fontconfig.h>
#endif

#include "common.h"
#include "cli.h"
#include "pinger.h"
#include "parser.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/appbar.h"
#include "ui/notifier.h"
#include "tabs/ping.h"
#include "tabs/graph.h"
#include "tabs/log.h"

#define APPFLAGS G_APPLICATION_NON_UNIQUE
#define TAB_BGTYPE(tab) { bg_light = ((tab) == graphtab_ref); }

static GtkWidget* graphtab_ref;
static int exit_code = EXIT_SUCCESS;

static void on_app_exit(GtkWidget *widget, gpointer unused) {
// note: subprocesses have to be already terminated by system at this point
// if not, then pinger_on_quit(true);
  logtab_clear();
  graphtab_free();
  pinger_on_quit(false);
#ifdef FCFINI
  FcFini();
#endif
}

static void on_tab_switch(GtkNotebook *nb, GtkWidget *tab, guint ndx, gpointer unused) {
  TAB_BGTYPE(tab);
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

#define APPQUIT(fmt, ...) { WARN("init " fmt " failed", __VA_ARGS__); \
  g_application_quit(G_APPLICATION(app)); exit_code = EXIT_FAILURE; return; }

static void app_cb(GtkApplication* app, gpointer unused) {
  style_init();
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
  t_tab* tabs[TAB_NDX_MAX] = {
    [TAB_PING_NDX]  = pingtab_init(win),
    [TAB_GRAPH_NDX] = opts.graph ? graphtab_init(win) : NULL,
    [TAB_LOG_NDX]   = logtab_init(win),
  };
  if (tabs[TAB_GRAPH_NDX]) graphtab_ref = tabs[TAB_GRAPH_NDX]->tab;
  for (int i = 0; i < G_N_ELEMENTS(tabs); i++) {
    gboolean graph = (i == TAB_GRAPH_NDX);
    t_tab *tab = tabs[i];
    if (!tab || !tab->tab || !tab->lab) { if (graph && !opts.graph) continue; else APPQUIT("tab#%d", i); }
    tab_setup(tab, graph ? CSS_LIGHT_BG : NULL);
    int ndx = gtk_notebook_append_page(GTK_NOTEBOOK(nb), tab->tab, tab->lab);
    if (ndx < 0) APPQUIT("tab page#%d", i);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(nb), tab->tab, true);
  }
  gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), start_page);
  if (graphtab_ref) gtk_widget_set_visible(graphtab_ref, opts.graph);
  if (start_page != TAB_GRAPH_NDX) TAB_BGTYPE(tabs[start_page]->tab)
  else if (graphtab_ref) TAB_BGTYPE(graphtab_ref);
  g_signal_connect(nb, EV_TAB_SWITCH, G_CALLBACK(on_tab_switch), NULL);
  // nb overlay
  GtkWidget *over = notifier_init(NT_MAIN_NDX, nb);
  gboolean with_over = GTK_IS_OVERLAY(over);
  if (!with_over) LOG_("failed to add overlay");
  gtk_window_set_child(GTK_WINDOW(win), with_over ? over : nb);
  //
  g_signal_connect(win, EV_DESTROY, G_CALLBACK(on_app_exit), NULL);
  gtk_window_present(GTK_WINDOW(win));
  { char *ver; // log runtime versions
    LOG("%c%s: " VERSION, g_ascii_toupper(APPNAME[0]), &(APPNAME[1]));
    if ((ver = rtver(GTK_STRV)))   { LOG("GTK: %s", ver);   g_free(ver); }
    if ((ver = rtver(CAIRO_STRV))) { LOG("Cairo: %s", ver); g_free(ver); }
    if ((ver = rtver(PANGO_STRV))) { LOG("Pango: %s", ver); g_free(ver); }
  }
  if (autostart && opts.target) { pinger_start(); appbar_update(); }
}

static void recap_cb(GApplication* app, gpointer unused) {
  g_timeout_add_seconds(1, (GSourceFunc)pinger_recap_cb, app);
  if (G_IS_APPLICATION(app)) g_application_hold(app);
  pinger_start();
  return;
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, ""); // l10n for CLI option parser
  g_return_val_if_fail(parser_init(), EX_SOFTWARE);
  g_return_val_if_fail(cli_init(&argc, &argv), EX_USAGE);
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
  g_object_unref(app);
  return exit_code || rc;
}

