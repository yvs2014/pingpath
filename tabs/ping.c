
#include "ping.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/clipboard.h"
#include "ui/option.h"

#define ELEM_BUFF_SIZE 16
#define HDRLINES 1

#ifdef WITH_DND
typedef struct listline t_listline;

typedef struct pt_dnd {
  GtkWidget *w, *b;
  t_listline *hdr, *body;
  const t_elem_desc *desc;
  int dx, dy;
} t_pt_dnd;
#endif

typedef struct listline {
  GtkListBoxRow *row;
  GtkWidget *box;            // row child
  GtkWidget* labs[ELEM_MAX]; // cache of box labels
#ifdef WITH_DND
  t_pt_dnd dnd[ELEM_MAX];
#endif
} t_listline;

typedef struct listbox {
  t_listline hdrlines[HDRLINES];
  t_listline bodylines[MAXTTL];
  t_listline info;
} t_listbox;

static t_listbox listbox;

static t_tab pingtab = { .self = &pingtab, .name = "ping-tab",
  .tag = PING_TAB_TAG, .tip = PING_TAB_TIP, .ico = {PING_TAB_ICON, PING_TAB_ICOA, PING_TAB_ICOB},
  .desc = { [POP_MENU_NDX_COPY] = { .name = "win.ping_menu_copy" }, [POP_MENU_NDX_SALL] = { .name = "win.ping_menu_sall" }},
  .act = { [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l2 }, [POP_MENU_NDX_SALL] = { .activate = cb_on_sall }},
};

static void pt_align_elem_label(GtkWidget* label, GtkAlign align, gboolean expand) {
  gtk_widget_set_halign(label, align);
  gtk_label_set_xalign(GTK_LABEL(label), align == GTK_ALIGN_END);
  gtk_widget_set_hexpand(label, expand);
}

static void pt_set_elem_align(int type, GtkWidget *label) {
  switch (type) {
    case ELEM_HOST:
    case ELEM_AS:
    case ELEM_CC:
    case ELEM_DESC:
    case ELEM_RT:
      pt_align_elem_label(label, GTK_ALIGN_START, false);
      break;
    case ELEM_FILL:
      pt_align_elem_label(label, GTK_ALIGN_START, true);
      break;
    case ELEM_LOSS:
    case ELEM_SENT:
    case ELEM_RECV:
    case ELEM_LAST:
    case ELEM_BEST:
    case ELEM_WRST:
    case ELEM_AVRG:
    case ELEM_JTTR:
    case ELEM_NO:
      pt_align_elem_label(label, GTK_ALIGN_END, false);
      break;
  }
  gtk_widget_set_valign(label, GTK_ALIGN_START);
}

#ifdef WITH_DND
#ifdef DNDORD_DEBUGGING
static void pt_pr_elem_ndxs(const char *pre, const t_type_elem *elems, int len) {
  GString *s = g_string_new(NULL);
  for (int i = 0; i < len; i++) g_string_append_printf(s, " %d", elems[i].type);
  g_message("REORDER %s%s", pre, s->str); g_string_free(s, true);
}
#define PT_REORDER_PRINT_ELEMS(pre, elems, len) { if (verbose & 32) pt_pr_elem_ndxs(pre, elems, len); }
#else
#define PT_REORDER_PRINT_ELEMS(...) {}
#endif

static gboolean pt_reorder_elems(int prev, int next, const t_elem_desc *desc) {
  if ((prev == next) || !desc) return false;
  int prev1 = prev - desc->mm.min, next1 = next - desc->mm.min, len = desc->mm.max - desc->mm.min + 1;
  if ((len <= 0) || (prev1 < 0) || (prev1 >= len) || (next1 < 0) || (next1 > len)) return false;
  //
  t_type_elem *elems = &(desc->elems[desc->mm.min]);
  t_type_elem *el = g_memdup2(elems, len * sizeof(t_type_elem));
  if (!el) { FAIL("g_memdup2()"); return false; }
  int i = 0;
  for (int j = 0; j < next1; j++) if (j != prev1) el[i++] = elems[j];
  el[i++] = elems[prev1];
  for (int j = next1; j < len; j++) if (j != prev1) el[i++] = elems[j];
  memcpy(elems, el, len * sizeof(t_type_elem));
  g_free(el);
  return true;
}

static GtkWidget* pt_box_nth_label(GtkWidget *box, int n) {
  if (GTK_IS_BOX(box)) {
    GtkWidget *w = gtk_widget_get_first_child(box);
    for (int i = 0; w && (i < ELEM_MAX); i++) {
      if (i == n) return w;
      w = gtk_widget_get_next_sibling(w);
    }
  }
  return NULL;
}

static int pt_box_wndx(GtkWidget *box, GtkWidget *widget) {
  if (GTK_IS_BOX(box) && GTK_IS_WIDGET(widget)) {
    GtkWidget *w = gtk_widget_get_first_child(box);
    for (int i = 0; w && (i < ELEM_MAX); i++) {
      if (w == widget) return i;
      w = gtk_widget_get_next_sibling(w);
    }
  }
  return -1;
}

static void pt_recache_labels(GtkWidget *box, GtkWidget **cells) {
  GtkWidget *w = gtk_widget_get_first_child(box);
  for (int i = 0; w && (i < ELEM_MAX); i++) { cells[i] = w; w = gtk_widget_get_next_sibling(w); }
}

static gboolean pt_reorder_cells(t_listline *lines, int len, int sn, int dn, gboolean before) {
  gboolean ok = true;
  for (int i = 0; i < len; i++) { // reorder line elements
    GtkWidget *box = lines[i].box;
    GtkWidget *slab = pt_box_nth_label(box, sn), *dlab = pt_box_nth_label(box, dn);
    if (GTK_IS_BOX(box) && GTK_IS_WIDGET(slab) && GTK_IS_WIDGET(dlab)) {
      gtk_box_reorder_child_after(GTK_BOX(box), slab, before ? gtk_widget_get_prev_sibling(dlab) : dlab);
      memset(lines[i].labs, 0, sizeof(lines[i].labs));
      pt_recache_labels(box, lines[i].labs);
      continue;
    } else WARN("missed widgets at line=%d [src=%d,dst=%d]", i, sn, dn);
    if (ok) ok = false;
  }
  return ok;
}

#define PT_LAB_TEXT(lab) (GTK_IS_LABEL(lab) ? gtk_label_get_text(GTK_LABEL(lab)) : "")

static GdkContentProvider* pt_hdr_dnd_drag(GtkDragSource *src, gdouble x, gdouble y, t_pt_dnd *dnd) {
  if (!dnd) return NULL;
  dnd->dx = round(x); dnd->dy = round(y);
  DNDORD("DND-drag[%s]: dx,dy=(%d,%d)", PT_LAB_TEXT(dnd->w), dnd->dx, dnd->dy);
  return gdk_content_provider_new_typed(G_TYPE_POINTER, dnd);
}

static void pt_hdr_dnd_icon(GtkDragSource *src, GdkDrag *drag, t_pt_dnd *dnd) {
  if (!GTK_IS_DRAG_SOURCE(src) || !dnd || !GTK_IS_WIDGET(dnd->w)) return;
  GdkPaintable *icon = gtk_widget_paintable_new(dnd->w);
  if (!GDK_IS_PAINTABLE(icon)) return;
  gtk_drag_source_set_icon(src, icon, dnd->dx, dnd->dy);
  g_object_unref(icon);
  DNDORD("DND-icon[%s]: dx,dy=(%d,%d)", PT_LAB_TEXT(dnd->w), dnd->dx, dnd->dy);
}

static GdkDragAction pt_hdr_on_move(GtkDropTarget *dst, gdouble x, gdouble y, t_pt_dnd *ddnd) {
  if (!GTK_IS_DROP_TARGET(dst) || !ddnd) return 0;
  const GValue *val = gtk_drop_target_get_value(dst);
  t_pt_dnd *sdnd = (val && G_VALUE_HOLDS_POINTER(val)) ? g_value_get_pointer(val) : NULL;
  return ((sdnd != ddnd) && sdnd && (sdnd->desc == ddnd->desc)) ? GDK_ACTION_COPY : 0;
}

static gboolean pt_hdr_on_drop(GtkDropTarget *dst, const GValue *val, gdouble x, gdouble y, t_pt_dnd *ddnd) {
  t_pt_dnd *sdnd = (val && G_VALUE_HOLDS_POINTER(val)) ? g_value_get_pointer(val) : NULL;
  if (!sdnd || !ddnd || !ddnd->desc || !ddnd->hdr || !ddnd->body) return false;
  int sn = pt_box_wndx(ddnd->b, sdnd->w), dn = pt_box_wndx(ddnd->b, ddnd->w);
  if ((sn < 0) || (dn < 0) || (sn == dn)) return false;
  if (!GTK_IS_WIDGET(sdnd->w) || !GTK_IS_WIDGET(ddnd->w) || (sdnd->w == ddnd->w)) return false;
  if (sdnd->desc != ddnd->desc) { DNDORD("DND-drop: %s", "different groups"); return false; }
  gboolean before = gtk_widget_contains(ddnd->w, x * 2, y);
  PT_REORDER_PRINT_ELEMS("array: <<<", ddnd->desc->elems, ELEM_MAX);
  PT_REORDER_PRINT_ELEMS("group: <<<", &(ddnd->desc->elems[ddnd->desc->mm.min]), ddnd->desc->mm.max - ddnd->desc->mm.min + 1);
  if (pt_reorder_elems(sn, before ? dn : dn + 1, ddnd->desc)) { // reorder inplace asserting no further errors
    if (!pt_reorder_cells(ddnd->hdr, HDRLINES, sn, dn, before))
      g_critical("REORDER header: partial reordering");
    if (!pt_reorder_cells(ddnd->body, MAXTTL, sn, dn, before))
      g_critical("REORDER lines: partial reordering");
    GString *msg = g_string_new(NULL);
    if (msg) {
      g_string_printf(msg, "\"%s\" %s \"%s\"", PT_LAB_TEXT(sdnd->w), before ? "before" : "after", PT_LAB_TEXT(ddnd->w));
      DNDORD("DND-drop: put %s", msg->str); LOG("dnd reorder: %s", msg->str); g_string_free(msg, true);
    }
  }
  PT_REORDER_PRINT_ELEMS("group: >>>", &(ddnd->desc->elems[ddnd->desc->mm.min]), ddnd->desc->mm.max - ddnd->desc->mm.min + 1);
  PT_REORDER_PRINT_ELEMS("array: >>>", ddnd->desc->elems, ELEM_MAX);
  option_up_menu_main();
  return true;
}
#endif

static gboolean is_statinfo_ndx(int ndx) {
  int type = pingelem[ndx].type; return ((type != ELEM_NO) && (type != ELEM_FILL));
}

static gboolean pt_init_line_elems(t_type_elem *elem, t_listline *line,
    t_listline *bodylines, GtkSizeGroup* group[ELEM_MAX]) {
  if (!line) return false;
  gboolean vis_n_dnd = (bodylines != NULL);
  for (int i = 0; i < ELEM_MAX; i++) {
    GtkWidget *label = line->labs[i] = gtk_label_new(elem[i].name);
    g_return_val_if_fail(label, false);
    if (group[i]) gtk_size_group_add_widget(group[i], label);
    pt_set_elem_align(pingelem[i].type, label);
    gtk_widget_set_visible(label, vis_n_dnd && elem[i].enable);
    gtk_widget_set_tooltip_text(label, elem[i].tip);
    gtk_box_append(GTK_BOX(line->box), label);
#ifdef WITH_DND
    t_pt_dnd *dnd = &(line->dnd[i]);
    dnd->w = label; dnd->b = line->box; dnd->hdr = line; dnd->body = bodylines;
    dnd->desc = IS_INFO_NDX(i) ? &info_desc : (IS_STAT_NDX(i) ? &stat_desc : NULL);
    if (vis_n_dnd && is_statinfo_ndx(i)) {
      GtkDragSource *dnd_src = gtk_drag_source_new();
      if (dnd_src) {
        g_signal_connect(dnd_src, EV_DND_DRAG, G_CALLBACK(pt_hdr_dnd_drag), dnd);
        g_signal_connect(dnd_src, EV_DND_ICON, G_CALLBACK(pt_hdr_dnd_icon), dnd);
        gtk_widget_add_controller(label, GTK_EVENT_CONTROLLER(dnd_src));
      }
      GtkDropTarget *dnd_dst = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_COPY);
      if (dnd_dst) {
        gtk_drop_target_set_preload(dnd_dst, true);
        g_signal_connect(dnd_dst, EV_DND_MOVE, G_CALLBACK(pt_hdr_on_move), dnd);
        g_signal_connect(dnd_dst, EV_DND_DROP, G_CALLBACK(pt_hdr_on_drop), dnd);
        gtk_widget_add_controller(label, GTK_EVENT_CONTROLLER(dnd_dst));
      }
    }
#endif
  }
  return true;
}

static GtkWidget* pt_init_list_box(t_listline *lines, int len, t_type_elem *elems,
    t_listline* bodylines, GtkSizeGroup* group[ELEM_MAX]) {
  static char stat_no_at_buff[MAXTTL][ELEM_BUFF_SIZE];
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(list, NULL);
  gtk_list_box_set_show_separators(GTK_LIST_BOX(list), true);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  t_type_elem bodyelem[G_N_ELEMENTS(pingelem)]; memcpy(bodyelem, pingelem, sizeof(bodyelem));
  for (int i = 0; i < len; i++) {
    GtkWidget *box = lines[i].box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_return_val_if_fail(box, NULL);
    if (style_loaded) gtk_widget_add_css_class(box, CSS_PAD6);
    gtk_widget_set_visible(box, elems != NULL);
    GtkListBoxRow *row = lines[i].row = line_row_new(box, elems != NULL);
    g_return_val_if_fail(row, NULL);
    if (elems) { // header
      if (!pt_init_line_elems(elems, &lines[i], bodylines, group)) return NULL;
    } else {     // line with number
      char *s = stat_no_at_buff[i]; g_snprintf(s, ELEM_BUFF_SIZE, "%d.", i + 1);
      bodyelem[ELEM_NO].name = s;
      if (!pt_init_line_elems(bodyelem, &lines[i], NULL, group)) return NULL;
    }
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
  }
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  return list;
}

static GtkWidget* pt_init_info(void) {
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(list, NULL);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  GtkWidget *label = listbox.info.box = gtk_label_new(NULL);
  g_return_val_if_fail(label, NULL);
  gtk_widget_set_visible(label, false);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  listbox.info.row = line_row_new(label, false);
  g_return_val_if_fail(listbox.info.row, NULL);
  gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(listbox.info.row));
  return list;
}

static void pt_set_vis_cells(GtkWidget **labs) {
  if (labs) for (int i = 0; i < ELEM_MAX; i++) if (GTK_IS_WIDGET(labs[i]))
    gtk_widget_set_visible(labs[i], pingelem[i].enable);
}


// pub
//

void pingtab_clear(void) {
  for (int i = 0; i < MAXTTL; i++) {
    t_listline *line = &listbox.bodylines[i];
    if (!GTK_IS_WIDGET(line->row) || !GTK_IS_BOX(line->box)) continue;
    gtk_widget_set_visible(GTK_WIDGET(line->row), false);
    gtk_widget_set_visible(line->box, false);
    for (int j = 0; j < ELEM_MAX; j++) {
      GtkWidget *lab = line->labs[j];
      if (GTK_IS_LABEL(lab)) {
        gtk_widget_set_visible(lab, false);
        if (is_statinfo_ndx(j)) gtk_label_set_text(GTK_LABEL(lab), NULL);
      }
    }
  }
  gtk_widget_set_visible(GTK_WIDGET(listbox.info.row), false);
  gtk_widget_set_visible(listbox.info.box, false);
  gtk_label_set_label(GTK_LABEL(listbox.info.box), NULL);
}

void pingtab_update(void) {
  if (!pinger_state.pause) for (int i = 0; i < tgtat; i++) {
    t_listline *line = &listbox.bodylines[i];
    for (int j = 0; j < ELEM_MAX; j++) {
      GtkWidget *lab = line->labs[j];
      if (GTK_IS_LABEL(lab) && is_statinfo_ndx(j) && pingelem[j].enable) {
        const char *text = stat_str_elem(i, pingelem[j].type);
        UPDATE_LABEL(lab, text);
      }
    }
  }
}

void pingtab_vis_rows(int upto) {
  LOG("set upto %d visible rows", upto);
  for (int i = 0; i < MAXTTL; i++) {
    gboolean vis = (i >= opts.min) && (i < upto);
    t_listline *line = &listbox.bodylines[i];
    if (GTK_IS_WIDGET(line->row)) gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
    if (GTK_IS_WIDGET(line->box)) gtk_widget_set_visible(line->box, vis);
    for (int j = 0; j < ELEM_MAX; j++) if (GTK_IS_LABEL(line->labs[j]))
      gtk_widget_set_visible(line->labs[j], vis && pingelem[j].enable);
  }
}

void pingtab_vis_cols(void) {
  DEBUG("set %s", "visible columns");
  for (int i = 0; i < HDRLINES; i++) pt_set_vis_cells(listbox.hdrlines[i].labs);
  for (int i = 0; i < MAXTTL; i++) pt_set_vis_cells(listbox.bodylines[i].labs);
  pingtab_update();
}

void pingtab_set_error(const char *error) {
  gtk_label_set_label(GTK_LABEL(listbox.info.box), error);
  gboolean vis = error && error[0];
  gtk_widget_set_visible(GTK_WIDGET(listbox.info.row), vis);
  gtk_widget_set_visible(listbox.info.box, vis);
}

t_tab* pingtab_init(GtkWidget* win) {
  TW_TW(pingtab.lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  g_return_val_if_fail(pingtab.lab.w, NULL);
  TW_TW(pingtab.tab, gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN), CSS_PAD, CSS_BGROUND);
  g_return_val_if_fail(pingtab.tab.w, NULL);
  //
  GtkSizeGroup* group[ELEM_MAX];
  for (int i = 0; i < G_N_ELEMENTS(group); i++) {
    group[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    g_return_val_if_fail(group[i], NULL);
  }
  TW_TW(pingtab.dyn, pt_init_list_box(listbox.bodylines, MAXTTL, NULL, NULL, group), NULL, CSS_BGROUND);
  g_return_val_if_fail(GTK_IS_LIST_BOX(pingtab.dyn.w), NULL);
  //
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(box, NULL);
  gtk_box_append(GTK_BOX(box), pingtab.dyn.w);
  TW_TW(pingtab.info, pt_init_info(), NULL, CSS_BGROUND);
  g_return_val_if_fail(GTK_IS_WIDGET(pingtab.info.w), NULL);
  gtk_box_append(GTK_BOX(box), pingtab.info.w);
  //
  TW_TW(pingtab.hdr, pt_init_list_box(listbox.hdrlines, HDRLINES, pingelem, listbox.bodylines, group), NULL, CSS_BGROUND);
  g_return_val_if_fail(GTK_IS_LIST_BOX(pingtab.hdr.w), NULL);
  gtk_box_append(GTK_BOX(pingtab.tab.w), pingtab.hdr.w);
  for (int i = 0; i < G_N_ELEMENTS(group); i++) if (GTK_IS_SIZE_GROUP(group[i])) g_object_unref(group[i]);
  //
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(scroll, NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(pingtab.tab.w), scroll);
  if (!clipboard_init(win, &pingtab)) LOG("no %s clipboard", pingtab.name);
  return &pingtab;
}

