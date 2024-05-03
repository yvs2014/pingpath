
#include "clipboard.h"

static const char* cb_menu_label(int sel) {
  return (sel < 0) ? ACT_COPY_HDR : (sel ? ACT_UNSALL_HDR : ACT_SALL_HDR);
}

static void cb_menu_update(t_tab *tab) {
  if (!tab || !G_IS_MENU(tab->menu)) return;
  g_menu_remove_all(tab->menu);
  for (int i = 0; i < POP_MENU_NDX_MAX; i++) {
    GMenuItem *item = g_menu_item_new(cb_menu_label((i == POP_MENU_NDX_SALL) ? tab->sel : -1), tab->desc[i].name);
    if (item) { g_menu_append_item(tab->menu, item); g_object_unref(item); }
    else FAIL("g_menu_item_new()");
  }
  SET_SA(tab->desc, POP_MENU_NDX_COPY, tab->sel);
}

static GtkWidget* cb_popover_init(GtkWidget *win, t_tab *tab) {
  g_return_val_if_fail(GTK_IS_WINDOW(win) && tab, NULL);
  tab->menu = g_menu_new();
  g_return_val_if_fail(G_IS_MENU(tab->menu), NULL);
  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(tab->menu));
  g_return_val_if_fail(GTK_IS_POPOVER(popover), NULL);
  for (int i = 0; i < POP_MENU_NDX_MAX; i++) tab->act[i].name = tab->desc[i].name + ACT_DOT;
  g_action_map_add_action_entries(G_ACTION_MAP(win), tab->act, POP_MENU_NDX_MAX, tab);
  for (int i = 0; i < POP_MENU_NDX_MAX; i++)
    tab->desc[i].sa = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(win), tab->act[i].name));
  gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
  gtk_popover_set_has_arrow(GTK_POPOVER(popover), false);
  gtk_widget_set_halign(popover, GTK_ALIGN_START);
  cb_menu_update(tab);
  return popover;
}

static void cb_popover_show(GtkWidget *popover, double x, double y) {
  if (!GTK_IS_POPOVER(popover)) return;
  if ((x < 0) || (y < 0)) gtk_popover_set_pointing_to(GTK_POPOVER(popover), NULL);
  else {
    GdkRectangle r = {x, y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &r);
  }
  gtk_popover_popup(GTK_POPOVER(popover));
}

static void cb_on_press(GtkGestureClick *g, int n, double x, double y, GtkWidget *pop) {
  if (!GTK_IS_GESTURE_CLICK(g) || !GTK_IS_POPOVER(pop)) return;
  GdkEvent *ev = gtk_gesture_get_last_event(GTK_GESTURE(g),
    gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(g)));
  if (ev && gdk_event_triggers_context_menu(ev)) {
    gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_CLAIMED);
    cb_popover_show(pop, x, y);
  }
}

static gboolean any_list_sel(t_tab *tab) {
  if (tab) {
    GtkWidget* list[] = {tab->hdr.w, tab->dyn.w, tab->info.w};
    static int n_alsel = G_N_ELEMENTS(list);
    for (int i = 0; i < n_alsel; i++) if (GTK_IS_LIST_BOX(list[i])) {
      GList *l = gtk_list_box_get_selected_rows(GTK_LIST_BOX(list[i]));
      if (l) { g_list_free(l); return true; }
    }
  }
  return false;
}

static void cb_on_sel(GtkListBox* list, t_tab *tab) { if (tab) { tab->sel = any_list_sel(tab); cb_menu_update(tab); }}

static char* c_strjoinv(const char delim, const char **array) {
  if (!array) return NULL;
  if (!*array) return g_strdup("");
  int sz = 1; for (const char **p = array; *p; p++) sz += strlen(*p) + 1;
  char *s = g_malloc0(sz);
  char *d = g_stpcpy(s, *array);
  for (const char **p = array + 1; *p; p++) {
    *d++ = delim;
    d = g_stpcpy(d, *p);
  }
  return s;
}

static char* cb_collect_level1(GList *list) {
  const char *lines[DEF_LOGMAX + 1] = {NULL};
  int i = 0;
  for (GList *p = list; p && (i < DEF_LOGMAX); p = p->next) {
    GtkListBoxRow *row = p->data;
    if (GTK_IS_LIST_BOX_ROW(row)) {
      GtkWidget *label = gtk_list_box_row_get_child(row);
      if (GTK_IS_LABEL(label)) // text at level1
        lines[i++] = gtk_label_get_text(GTK_LABEL(label));
    }
  }
  return (i > 0) ? c_strjoinv('\n', lines) : NULL;
}

#define MAXT2 (MAXTTL * 2)

#define C_NULL2SP(list, n) for (int i = 0; i < MAXT2; i++) { \
  gboolean empty = true; \
  for (int j = 0; j < (ELEM_MAX + 1); j++) if ((list)[i][j]) { empty = false; break; } \
  if (empty) break; \
  for (int j = 0; j < (ELEM_MAX + 1); j++) if ((list)[i][j]) break; else (list)[i][j] = g_strdup_printf("%-*s", (n), ""); \
}

#define C_FREE_T2LIST(list) \
  for (int i = 0; i < MAXT2; i++) \
    for (int j = 0; j < (ELEM_MAX + 1); j++) \
      if ((list)[i][j]) g_free((list)[i][j]);

static void cb_calc_lengths(GList *list, int *maxes) {
  for (GList *p = list; p; p = p->next) {
    GtkListBoxRow *row = p->data; if (!GTK_IS_LIST_BOX_ROW(row)) continue;
    GtkWidget *box = gtk_list_box_row_get_child(row); if (!GTK_IS_BOX(box) || !gtk_widget_get_visible(box)) continue;
    int j = 0;
    for (GtkWidget *lab = gtk_widget_get_first_child(box); GTK_IS_LABEL(lab) && (j < ELEM_MAX);
         lab = gtk_widget_get_next_sibling(lab)) {
      if (!gtk_widget_get_visible(lab)) continue;
      const char *str = gtk_label_get_text(GTK_LABEL(lab));
      if (!str) continue;
      if (str[0]) { int l = g_utf8_strlen(str, MAXHOSTNAME); if (l > maxes[j]) maxes[j] = l; }
      j++;
    }
  }
}

static void cb_collect_maxes(GtkWidget *box, int *maxes) {
  if (!GTK_IS_LIST_BOX(box) || !maxes) return;
  GList *list = gtk_list_box_get_selected_rows(GTK_LIST_BOX(box));
  if (list) cb_calc_lengths(list, maxes);
}

static char* cb_collect_level2(GList *list, int *maxes) {
  char* elems[MAXT2][ELEM_MAX + 1] = {{NULL}}; // *2: supposedly it's enough for multihop lines
  int i = 0;
  for (GList *p = list; p && (i < MAXT2); p = p->next) {
    GtkListBoxRow *row = p->data; if (!GTK_IS_LIST_BOX_ROW(row)) continue;
    GtkWidget *box = gtk_list_box_row_get_child(row); if (!GTK_IS_BOX(box) || !gtk_widget_get_visible(box)) continue;
    int j = 0, di = 0;
    for (GtkWidget *lab = gtk_widget_get_first_child(box); GTK_IS_LABEL(lab) && (j < ELEM_MAX);
         lab = gtk_widget_get_next_sibling(lab)) {
      if (!gtk_widget_get_visible(lab)) continue;
      const char *str = gtk_label_get_text(GTK_LABEL(lab));
      if (!str) continue;
      int l = maxes ? maxes[j] : 0;
      if (str[0]) {
        char **multi = g_strsplit(str, "\n", MAXADDR);
        if (multi) {
          int ii = i;
          for (char **s = multi; *s && (ii < MAXT2); s++) // put '\n'-elems ahead
            elems[ii++][j] = (l > 0) ? g_strdup_printf("%-*s", l, *s) : g_strdup(*s);
          g_strfreev(multi);
          if ((ii - i) > di) di = ii - i;
        }
      } else elems[i][j] = (l > 0) ? g_strdup_printf("%-*s", l, str) : g_strdup(str);
      j++;
    }
    i += di;
  }
  C_NULL2SP(elems, 3); // if there are multihop lines
  char *lines[MAXT2 + 1] = {NULL};
  for (int i = 0; i < MAXT2; i++) // combine collected elems into lines
    if (elems[i][0]) lines[i] = g_strjoinv(" ", elems[i]);
  C_FREE_T2LIST(elems); // free elems
  char *text = g_strjoinv("\n", lines); // combines lines into table
  for (int i = 0; i < (MAXT2 + 1); i++) // free lines
    g_free(lines[i]);
  return text;
}

enum { DATA_AT_LEVEL1, DATA_AT_LEVEL2 };

static char* cb_get_text(GtkWidget* lb, GdkClipboard *cb, int level, int *maxes) {
  if (!GTK_IS_LIST_BOX(lb) || !GDK_IS_CLIPBOARD(cb)) return NULL;
  GList *list = gtk_list_box_get_selected_rows(GTK_LIST_BOX(lb));
  if (!list) return NULL;
  char *text = NULL;
  if (g_list_length(list)) switch (level) {
    case DATA_AT_LEVEL1: text = cb_collect_level1(list); break;
    case DATA_AT_LEVEL2: text = cb_collect_level2(list, maxes); break;
  }
  g_list_free(list);
  return text;
}


// pub
//

gboolean clipboard_init(GtkWidget *win, t_tab *tab) {
  g_return_val_if_fail(GTK_IS_WINDOW(win) && tab && GTK_IS_BOX(tab->tab.w), false);
  GtkGesture *gest = gtk_gesture_click_new();
  g_return_val_if_fail(GTK_IS_GESTURE_CLICK(gest), false);
  tab->pop = cb_popover_init(win, tab);
  g_return_val_if_fail(GTK_IS_POPOVER(tab->pop), false);
  gtk_widget_set_parent(tab->pop, tab->tab.w);
  g_signal_connect(gest, EV_PRESS, G_CALLBACK(cb_on_press), tab->pop);
  GtkWidget* list[] = {tab->hdr.w, tab->dyn.w, tab->info.w};
  for (int i = 0; i < G_N_ELEMENTS(list); i++) if (GTK_IS_LIST_BOX(list[i]))
    g_signal_connect(list[i], EV_ROW_CHANGE, G_CALLBACK(cb_on_sel), tab);
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gest), 0);
  gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(gest), true);
  gtk_widget_add_controller(GTK_WIDGET(tab->tab.w), GTK_EVENT_CONTROLLER(gest));
  gtk_event_controller_set_propagation_limit(GTK_EVENT_CONTROLLER(gest), GTK_LIMIT_SAME_NATIVE);
  return true;
}

void cb_on_copy_l1(GSimpleAction *action, GVariant *var, void *data) {
  t_tab *tab = data;
  if (!tab || !GTK_IS_LIST_BOX(tab->dyn.w)) return;
  GdkClipboard *cb = gtk_widget_get_clipboard(GTK_WIDGET(tab->tab.w));
  if (GDK_IS_CLIPBOARD(cb)) {
    VERBOSE("clipoard action %s", ACT_COPY_HDR);
    char *text = cb_get_text(tab->dyn.w, cb, DATA_AT_LEVEL1, NULL);
    if (text) { gdk_clipboard_set_text(cb, text); g_free(text); }
  }
}

void cb_on_copy_l2(GSimpleAction *action, GVariant *var, void *data) {
  t_tab *tab = data;
  if (!tab || !GTK_IS_LIST_BOX(tab->dyn.w)) return;
  GdkClipboard *cb = gtk_widget_get_clipboard(GTK_WIDGET(tab->tab.w));
  if (GDK_IS_CLIPBOARD(cb)) {
    VERBOSE("clipoard action %s", ACT_COPY_HDR);
    char* arr[4] = { NULL };
    static int n_cbl2_arr = G_N_ELEMENTS(arr);
    int maxes[ELEM_MAX]; memset(maxes, 0, sizeof(maxes));
    cb_collect_maxes(tab->hdr.w, maxes);
    cb_collect_maxes(tab->dyn.w, maxes);
    int n = 0;
    arr[n] = cb_get_text(tab->hdr.w,  cb, DATA_AT_LEVEL2, maxes); if (arr[n]) n++;
    arr[n] = cb_get_text(tab->dyn.w,  cb, DATA_AT_LEVEL2, maxes); if (arr[n]) n++;
    arr[n] = cb_get_text(tab->info.w, cb, DATA_AT_LEVEL1, NULL);  if (arr[n]) n++;
    if (!n) return;
    char *text = g_strjoinv("\n", arr);
    for (int i = 0; i < n_cbl2_arr; i++) g_free(arr[i]);
    if (text) { gdk_clipboard_set_text(cb, text); g_free(text); }
  }
}

void cb_on_sall(GSimpleAction *action, GVariant *var, void *data) {
  t_tab *tab = data;
  if (!tab || !GTK_IS_LIST_BOX(tab->dyn.w)) return;
  VERBOSE("%s action %s", tab->name, cb_menu_label(tab->sel));
  GtkWidget* list[] = {tab->hdr.w, tab->dyn.w, tab->info.w};
  static int n_cbsa_arr = G_N_ELEMENTS(list);
  tab->sel = any_list_sel(tab);
  if (tab->sel) {
    for (int i = 0; i < n_cbsa_arr; i++)
      if (GTK_IS_LIST_BOX(list[i])) gtk_list_box_unselect_all(GTK_LIST_BOX(list[i]));
  } else {
    for (int i = 0; i < n_cbsa_arr; i++)
      if (GTK_IS_LIST_BOX(list[i])) gtk_list_box_select_all(GTK_LIST_BOX(list[i]));
  }
}

