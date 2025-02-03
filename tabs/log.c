
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"
#include "common.h"
#include "aux.h"
#include "pinger.h"
#include "ui/clipboard.h"

static int loglines;

static t_tab logtab = { .self = &logtab, .name = "log-tab",
  .ico = {LOG_TAB_ICON, LOG_TAB_ICOA},
  .desc = {
    [POP_MENU_NDX_COPY] = { .name = "win.log_menu_copy" },
    [POP_MENU_NDX_SALL] = { .name = "win.log_menu_sall" }},
  .act = {
    [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l1 },
    [POP_MENU_NDX_SALL] = { .activate = cb_on_sall    }},
};

static void logtab_add_line(const char *str) {
  GtkWidget *line = gtk_label_new(str);
  g_return_if_fail(line);
  gtk_widget_set_halign(line, GTK_ALIGN_START);
  gtk_list_box_append(GTK_LIST_BOX(logtab.dyn.w), line);
  loglines++;
  while (loglines > opts.logmax) {
    GtkWidget *line = gtk_widget_get_first_child(logtab.dyn.w);
    if (GTK_IS_LABEL(line)) gtk_label_set_text(GTK_LABEL(line), NULL);
    if (GTK_IS_WIDGET(line)) { gtk_list_box_remove(GTK_LIST_BOX(logtab.dyn.w), line); loglines--; } else break;
  }
}

static inline void logtab_add_text(const char *str) {
  if (atquit || !str || !GTK_IS_LIST_BOX(logtab.dyn.w)) return;
  if (strchr(str, '\n')) {
    char** lines = g_strsplit(SNAPBOX_HINT, "\n", opts.logmax);
    g_return_if_fail(lines);
    for (char **s = lines; *s; s++) logtab_add_line(*s);
    g_strfreev(lines);
  } else logtab_add_line(str);
}

static inline void logtab_set_dyn_props(GtkWidget *widget) {
  if (!GTK_IS_LIST_BOX(widget)) return;
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(widget), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(widget), false);
  gtk_widget_set_halign(widget, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(widget, false);
}


// pub
//

t_tab* logtab_init(GtkWidget* win) {
  logtab.tag = LOG_TAB_TAG;
  logtab.tip = LOG_TAB_TIP;
  if (!basetab_init(&logtab, gtk_list_box_new, NULL)) return NULL;
  logtab_set_dyn_props(logtab.dyn.w);
  if (!clipboard_init(win, &logtab)) LOG("no %s clipboard", logtab.name);
  return &logtab;
}

void log_add(const char *fmt, ...) {
  if (atquit && opts.recap) return;
  va_list ap;
  va_start(ap, fmt);
  char *str = g_strdup_vprintf(fmt, ap);
  logtab_add_text(str);
  g_free(str);
  va_end(ap);
}

