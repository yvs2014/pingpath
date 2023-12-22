
#include "log.h"
#include "common.h"
#include "pinger.h"

#define LC_LOG(ndx) NOLOG("log action %s", log_menu_label(ndx))

const char *log_empty  = "<empty>";
static t_tab logtab = { .ico = LOG_TAB_ICON, .tag = LOG_TAB_TAG };
static int loglines;

// aux
//

static void logtab_add(const gchar *str) {
  if (!str || !GTK_IS_LIST_BOX(logtab.dyn)) return;
  GtkWidget *line = gtk_label_new(str);
  if (GTK_IS_LABEL(line)) {
    gtk_widget_set_halign(line, GTK_ALIGN_START);
    gtk_list_box_append(GTK_LIST_BOX(logtab.dyn), line);
    loglines++;
  }
  if (loglines > opts.logmax) {
    GtkWidget *line = gtk_widget_get_first_child(logtab.dyn);
    if (GTK_IS_LABEL(line)) gtk_label_set_text(GTK_LABEL(line), NULL);
    if (GTK_IS_WIDGET(line)) { gtk_list_box_remove(GTK_LIST_BOX(logtab.dyn), line); loglines--; }
    if (G_IS_OBJECT(line)) g_object_run_dispose(G_OBJECT(line)); // safe to free?
  }
}

enum { LOG_CLIP_NDX_COPY, LOG_CLIP_NDX_SALL, LOG_CLIP_NDX_MAX };

static t_act_desc log_menu_desc[LOG_CLIP_NDX_MAX] = {
  [LOG_CLIP_NDX_COPY] = { .name = "win.log_menu_copy" },
  [LOG_CLIP_NDX_SALL] = { .name = "win.log_menu_sall" },
};

static void on_log_copy(GSimpleAction*, GVariant*, gpointer);
static void on_log_sall(GSimpleAction*, GVariant*, gpointer);

static GActionEntry log_menu_entries[LOG_CLIP_NDX_MAX] = {
  [LOG_CLIP_NDX_COPY] = { .activate = on_log_copy },
  [LOG_CLIP_NDX_SALL] = { .activate = on_log_sall },
};

static const char* log_menu_label(int ndx) {
  switch (ndx) {
    case LOG_CLIP_NDX_COPY: return ACT_COPY_HDR;
    case LOG_CLIP_NDX_SALL: return logtab.sel ? ACT_UNSALL_HDR : ACT_SALL_HDR;
  }
  return "";
}

static void log_put_into_clipboard(GList* list, GdkClipboard *cb) {
  if (!list || !GDK_IS_CLIPBOARD(cb)) return;
  gchar* lines[DEF_LOGMAX + 1] = {NULL}; // all nulls
  int i = 0;
  for (GList *p = list; p && (i < DEF_LOGMAX); p = p->next) {
    GtkListBoxRow *row = p->data;
    if (GTK_IS_LIST_BOX_ROW(row)) {
      GtkWidget *line = gtk_list_box_row_get_child(row);
      if (GTK_IS_LABEL(line)) {
        const gchar *l = gtk_label_get_text(GTK_LABEL(line));
        if (l) { lines[i] = g_strndup(l, BUFF_SIZE); if (!lines[i++]) break; }
      }
    }
  }
  gchar *text = g_strjoinv("\n", lines);
  if (text) { gdk_clipboard_set_text(cb, text); g_free(text); }
  for (gchar **p = lines; *p; p++) g_free(*p);
}

static void on_log_copy(GSimpleAction *action, GVariant *var, gpointer data) {
  t_tab *tab = data;
  if (!tab || !GTK_IS_LIST_BOX(tab->dyn)) return;
  LC_LOG(LOG_CLIP_NDX_COPY);
  GList *sel = gtk_list_box_get_selected_rows(GTK_LIST_BOX(tab->dyn));
  if (sel) {
    if (g_list_length(sel)) log_put_into_clipboard(sel, gtk_widget_get_clipboard(GTK_WIDGET(tab->tab)));
    g_list_free(sel);
  }
}

static void on_log_sall(GSimpleAction *action, GVariant *var, gpointer data) {
  t_tab *tab = data;
  if (!tab || !GTK_IS_LIST_BOX(tab->dyn)) return;
  LC_LOG(LOG_CLIP_NDX_SALL);
  GList *sel = gtk_list_box_get_selected_rows(GTK_LIST_BOX(tab->dyn));
  if (sel) { gtk_list_box_unselect_all(GTK_LIST_BOX(tab->dyn)); g_list_free(sel); }
  else gtk_list_box_select_all(GTK_LIST_BOX(tab->dyn));
}

static void log_show_popover(GtkWidget *popover, double x, double y) {
  if (!GTK_IS_POPOVER(popover)) return;
  if ((x < 0) || (y < 0)) gtk_popover_set_pointing_to(GTK_POPOVER(popover), NULL);
  else {
    GdkRectangle r = {x, y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &r);
  }
  gtk_popover_popup(GTK_POPOVER(popover));
}

static void on_log_click(GtkGestureClick *g, int n, double x, double y, GtkWidget *pop) {
  if (!GTK_IS_GESTURE_CLICK(g) || !GTK_IS_POPOVER(pop)) return;
  GdkEvent *ev = gtk_gesture_get_last_event(GTK_GESTURE(g),
    gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(g)));
  gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_CLAIMED);
  if (ev && gdk_event_triggers_context_menu(ev)) log_show_popover(pop, x, y);
}

static void log_menu_update(GMenu *menu) {
  if (!G_IS_MENU(menu)) return;
  g_menu_remove_all(menu);
  for (int i = 0; i < LOG_CLIP_NDX_MAX; i++)
    g_menu_append_item(menu, g_menu_item_new(log_menu_label(i), log_menu_desc[i].name));
  SET_SA(log_menu_desc, LOG_CLIP_NDX_COPY, logtab.sel);
}

static void on_log_sel(GtkListBox* list, GMenu *menu) {
  if (!GTK_IS_LIST_BOX(list)) return;
  GList *sel = gtk_list_box_get_selected_rows(GTK_LIST_BOX(list));
  logtab.sel = sel != NULL; g_list_free(sel);
  log_menu_update(menu);
}

static GtkWidget* log_popover_init(GtkWidget *win, t_tab *tab) {
  g_return_val_if_fail(GTK_IS_WINDOW(win) && tab, NULL);
  tab->menu = g_menu_new();
  g_return_val_if_fail(G_IS_MENU(tab->menu), NULL);
  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(tab->menu));
  g_return_val_if_fail(GTK_IS_POPOVER(popover), NULL);
  for (int i = 0; i < LOG_CLIP_NDX_MAX; i++) log_menu_entries[i].name = log_menu_desc[i].name + ACT_DOT;
  g_action_map_add_action_entries(G_ACTION_MAP(win), log_menu_entries, G_N_ELEMENTS(log_menu_entries), tab);
  for (int i = 0; i < LOG_CLIP_NDX_MAX; i++)
    log_menu_desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(win), log_menu_entries[i].name));
  gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
  gtk_popover_set_has_arrow(GTK_POPOVER(popover), false);
  gtk_widget_set_halign(popover, GTK_ALIGN_START);
  log_menu_update(tab->menu);
  return popover;
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
  // clipboard menu
  const char* warn_for = "for log menu";
  if (GTK_IS_WINDOW(win)) {
    GtkGesture *click = gtk_gesture_click_new();
    if (GTK_IS_GESTURE_CLICK(click)) {
      logtab.pop = log_popover_init(win, &logtab);
      if (GTK_IS_POPOVER(logtab.pop)) {
        gtk_widget_set_parent(logtab.pop, box);
        g_signal_connect(click, EV_CLICK, G_CALLBACK(on_log_click), logtab.pop);
        g_signal_connect(logtab.dyn, EV_ROW_CHANGE, G_CALLBACK(on_log_sel), logtab.menu);
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
        gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(click), true);
        gtk_widget_add_controller(GTK_WIDGET(logtab.dyn), GTK_EVENT_CONTROLLER(click));
      } else WARN("cannot get popover %s", warn_for);
    } else WARN("cannot create gesture %s", warn_for);
  } else WARN("no parent window %s", warn_for);
  //
  return &logtab;
}

void log_add(const gchar *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  gchar *str = g_strdup_vprintf(fmt, ap);
  if (str) { logtab_add(str); g_free(str); }
  va_end(ap);
}

