
#include "log.h"
#include "pinger.h"
#include "ui/clipboard.h"

const char *log_empty  = "<empty>";
static int loglines;

static t_tab logtab = { .self = &logtab, .name = "log-tab", .ico = LOG_TAB_ICON, .tag = LOG_TAB_TAG,
  .desc = { [POP_MENU_NDX_COPY] = { .name = "win.log_menu_copy" }, [POP_MENU_NDX_SALL] = { .name = "win.log_menu_sall" }},
  .act = { [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l1 },  [POP_MENU_NDX_SALL] = { .activate = cb_on_sall }},
};

static void logtab_add(const gchar *str) {
  if (!str || !GTK_IS_LIST_BOX(logtab.dyn)) return;
  GtkWidget *line = gtk_label_new(str);
  if (GTK_IS_LABEL(line)) {
    gtk_widget_set_halign(line, GTK_ALIGN_START);
    gtk_list_box_append(GTK_LIST_BOX(logtab.dyn), line);
    loglines++;
  }
  while (loglines > opts.logmax) {
    GtkWidget *line = gtk_widget_get_first_child(logtab.dyn);
    if (GTK_IS_LABEL(line)) gtk_label_set_text(GTK_LABEL(line), NULL);
    if (GTK_IS_WIDGET(line)) { gtk_list_box_remove(GTK_LIST_BOX(logtab.dyn), line); loglines--; } else break;
    if (G_IS_OBJECT(line)) g_object_run_dispose(G_OBJECT(line)); // safe to free?
  }
}


// pub
//

t_tab* logtab_init(GtkWidget* win) {
  logtab.lab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  g_return_val_if_fail(GTK_IS_BOX(logtab.lab), NULL);
  logtab.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(logtab.tab), NULL);
  logtab.dyn = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(logtab.dyn), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(logtab.dyn), false);
  g_return_val_if_fail(GTK_IS_LIST_BOX(logtab.dyn), NULL);
  gtk_widget_set_halign(logtab.dyn, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(logtab.dyn, false);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  gtk_box_append(GTK_BOX(box), logtab.dyn);
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(GTK_IS_SCROLLED_WINDOW(scroll), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(logtab.tab), scroll);
  if (!clipboard_init(win, &logtab)) LOG("no %s clipboard", logtab.name);
  return &logtab;
}

void log_add(const gchar *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  gchar *str = g_strdup_vprintf(fmt, ap);
  if (str) { logtab_add(str); g_free(str); }
  va_end(ap);
}

