
#include <math.h>

#include "ping.h"
#include "common.h"
#include "aux.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/clipboard.h"
#include "ui/option.h"

enum { ELEM_BUFF_SIZE = 16, HDRLINES = 1 };

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
  GtkWidget *box;          // row child
  GtkWidget* labs[PE_MAX]; // cache of box labels
#ifdef WITH_DND
  t_pt_dnd dnd[PE_MAX];
#endif
} t_listline;

typedef struct listbox {
  t_listline hdrlines[HDRLINES];
  t_listline bodylines[MAXTTL];
  t_listline info;
} t_listbox;

static t_listbox listbox;

static t_tab pingtab = { .self = &pingtab, .name = "ping-tab",
  .ico = {PING_TAB_ICON, PING_TAB_ICOA, PING_TAB_ICOB},
  .desc = {
    [POP_MENU_NDX_COPY] = { .name = "win.ping_menu_copy" },
    [POP_MENU_NDX_SALL] = { .name = "win.ping_menu_sall" }},
  .act = {
    [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l2 },
    [POP_MENU_NDX_SALL] = { .activate = cb_on_sall    }},
};

static void pt_align_elem_label(GtkWidget* label, GtkAlign align, gboolean expand) {
  gtk_widget_set_halign(label, align);
  gtk_label_set_xalign(GTK_LABEL(label), align == GTK_ALIGN_END);
  gtk_widget_set_hexpand(label, expand);
}

static void pt_set_elem_align(int type, GtkWidget *label) {
  switch (type) {
    case PE_HOST:
    case PE_AS:
    case PE_CC:
    case PE_DESC:
    case PE_RT:
      pt_align_elem_label(label, GTK_ALIGN_START, false);
      break;
    case PE_FILL:
      pt_align_elem_label(label, GTK_ALIGN_START, true);
      break;
    case PE_LOSS:
    case PE_SENT:
    case PE_RECV:
    case PE_LAST:
    case PE_BEST:
    case PE_WRST:
    case PE_AVRG:
    case PE_JTTR:
    case PE_NO:
      pt_align_elem_label(label, GTK_ALIGN_END, false);
      break;
    default: break;
  }
  gtk_widget_set_valign(label, GTK_ALIGN_START);
}

#ifdef WITH_DND
static void pt_pr_elem_ndxs(const char *pre, const char *dir, const t_type_elem *elems, int len) {
  GString *str = g_string_new(NULL);
  for (int i = 0; i < len; i++) g_string_append_printf(str, " %d", elems[i].type);
  g_message("%s: %s: %s %s", DEB_REORDER, pre, dir, str->str); g_string_free(str, true);
}

#define PT_PRINT_ELEMS(pre, dir, elems, len) do {         \
  if (verbose.dnd) pt_pr_elem_ndxs(pre, dir, elems, len); \
} while (0)

static gboolean pt_reorder_elems(int prev, int next, const t_elem_desc *desc) {
  if ((prev == next) || !desc) return false;
  int prev1 = prev - desc->mm.min, next1 = next - desc->mm.min, len = desc->mm.max - desc->mm.min + 1;
  if ((len <= 0) || (prev1 < 0) || (prev1 >= len) || (next1 < 0) || (next1 > len)) return false;
  //
  t_type_elem *elems = &desc->elems[desc->mm.min];
  t_type_elem *dup = g_memdup2(elems, len * sizeof(t_type_elem));
  if (!dup) return false;
  int ndx = 0;
  for (int i = 0; i < next1; i++) if (i != prev1) dup[ndx++] = elems[i];
  dup[ndx++] = elems[prev1];
  for (int i = next1; i < len; i++) if (i != prev1) dup[ndx++] = elems[i];
  memcpy(elems, dup, len * sizeof(t_type_elem)); // BUFFNOLINT
  g_free(dup);
  return true;
}

static GtkWidget* pt_box_nth_label(GtkWidget *box, int n) {
  if (GTK_IS_BOX(box)) {
    GtkWidget *widget = gtk_widget_get_first_child(box);
    for (int i = 0; widget && (i < PE_MAX); i++) {
      if (i == n) return widget;
      widget = gtk_widget_get_next_sibling(widget);
    }
  }
  return NULL;
}

static int pt_box_wndx(GtkWidget *box, GtkWidget *widget) {
  if (GTK_IS_BOX(box) && GTK_IS_WIDGET(widget)) {
    GtkWidget *item = gtk_widget_get_first_child(box);
    for (int i = 0; item && (i < PE_MAX); i++) {
      if (item == widget) return i;
      item = gtk_widget_get_next_sibling(item);
    }
  }
  return -1;
}

static void pt_recache_labels(GtkWidget *box, GtkWidget **cells) {
  GtkWidget *widget = gtk_widget_get_first_child(box);
  for (int i = 0; widget && (i < PE_MAX); i++) {
    cells[i] = widget;
    widget = gtk_widget_get_next_sibling(widget);
  }
}

static gboolean pt_reorder_cells(t_listline *lines, int len, int sn, int dn, gboolean before) {
  gboolean okay = true;
  for (int i = 0; i < len; i++) { // reorder line elements
    GtkWidget *box = lines[i].box;
    GtkWidget *slab = pt_box_nth_label(box, sn), *dlab = pt_box_nth_label(box, dn);
    if (GTK_IS_BOX(box) && GTK_IS_WIDGET(slab) && GTK_IS_WIDGET(dlab)) {
      gtk_box_reorder_child_after(GTK_BOX(box), slab, before ? gtk_widget_get_prev_sibling(dlab) : dlab);
      memset(lines[i].labs, 0, sizeof(lines[i].labs)); // BUFFNOLINT
      pt_recache_labels(box, lines[i].labs);
      continue;
    }
    DEBUG("%s: %s: #%d: %d -> %d", ERROR_HDR, DEB_REORDER, i, sn, dn);
    if (okay) okay = false;
  }
  return okay;
}

#define PT_LAB_TEXT(lab) (GTK_IS_LABEL(lab) ? gtk_label_get_text(GTK_LABEL(lab)) : "")

static GdkContentProvider* pt_hdr_dnd_drag(GtkDragSource *self G_GNUC_UNUSED,
    gdouble x, gdouble y, t_pt_dnd *dnd)
{
  if (!dnd) return NULL;
  dnd->dx = round(x); dnd->dy = round(y);
  DNDORD("%s[%s]: dx,dy=(%d,%d)", DEB_DND, PT_LAB_TEXT(dnd->w), dnd->dx, dnd->dy);
  return gdk_content_provider_new_typed(G_TYPE_POINTER, dnd);
}

static void pt_hdr_dnd_icon(GtkDragSource *src, GdkDrag *drag G_GNUC_UNUSED, t_pt_dnd *dnd) {
  if (!GTK_IS_DRAG_SOURCE(src) || !dnd || !GTK_IS_WIDGET(dnd->w)) return;
  GdkPaintable *icon = gtk_widget_paintable_new(dnd->w);
  if (!GDK_IS_PAINTABLE(icon)) return;
  gtk_drag_source_set_icon(src, icon, dnd->dx, dnd->dy);
  g_object_unref(icon);
  DNDORD("%s: %s: dx,dy=(%d,%d)", DEB_DND, PT_LAB_TEXT(dnd->w), dnd->dx, dnd->dy);
}

static GdkDragAction pt_hdr_on_move(GtkDropTarget *dst,
    gdouble x G_GNUC_UNUSED, gdouble y G_GNUC_UNUSED, t_pt_dnd *ddnd)
{
  if (!GTK_IS_DROP_TARGET(dst) || !ddnd)
    return 0; // ENUMNOLINT
  const GValue *val = gtk_drop_target_get_value(dst);
  t_pt_dnd *sdnd = (val && G_VALUE_HOLDS_POINTER(val)) ? g_value_get_pointer(val) : NULL;
  return (sdnd && (sdnd != ddnd) && (sdnd->desc == ddnd->desc)) ? GDK_ACTION_COPY : 0; // ENUMNOLINT
}

static gboolean pt_hdr_on_drop(GtkDropTarget *self G_GNUC_UNUSED, const GValue *value,
    gdouble x, gdouble y, t_pt_dnd *ddnd)
{
  t_pt_dnd *sdnd = (value && G_VALUE_HOLDS_POINTER(value)) ? g_value_get_pointer(value) : NULL;
  if (!sdnd || !ddnd || !ddnd->desc || !ddnd->hdr || !ddnd->body) return false;
  int sn = pt_box_wndx(ddnd->b, sdnd->w), dn = pt_box_wndx(ddnd->b, ddnd->w);
  if ((sn < 0) || (dn < 0) || (sn == dn)) return false;
  if (!GTK_IS_WIDGET(sdnd->w) || !GTK_IS_WIDGET(ddnd->w) || (sdnd->w == ddnd->w)) return false;
  if (sdnd->desc != ddnd->desc) { DNDORD("%s: %s", DEB_DND, "different groups"); return false; }
  gboolean before = gtk_widget_contains(ddnd->w, x * 2, y);
  PT_PRINT_ELEMS(ARRAY_HDR, "<<<", ddnd->desc->elems, PE_MAX);
  PT_PRINT_ELEMS(GROUP_HDR, "<<<", &ddnd->desc->elems[ddnd->desc->mm.min], ddnd->desc->mm.max - ddnd->desc->mm.min + 1);
  if (pt_reorder_elems(sn, before ? dn : dn + 1, ddnd->desc)) { // reorder inplace asserting no further errors
    if (!pt_reorder_cells(ddnd->hdr, HDRLINES, sn, dn, before))
      g_critical("%s: %s", DEB_REORDER, RDR_PART_ERR);
    if (!pt_reorder_cells(ddnd->body, MAXTTL, sn, dn, before))
      g_critical("%s: %s", DEB_REORDER, RDR_PART_ERR);
    GString *msg = g_string_new(NULL);
    if (msg) {
      g_string_printf(msg, "\"%s\" %s \"%s\"",
        PT_LAB_TEXT(sdnd->w), before ? BEFORE_HDR : AFTER_HDR, PT_LAB_TEXT(ddnd->w));
      LOG("%s: %s", DEB_REORDER, msg->str);
      g_string_free(msg, true);
    }
  }
  PT_PRINT_ELEMS(GROUP_HDR, ">>>", &ddnd->desc->elems[ddnd->desc->mm.min], ddnd->desc->mm.max - ddnd->desc->mm.min + 1);
  PT_PRINT_ELEMS(ARRAY_HDR, ">>>", ddnd->desc->elems, PE_MAX);
  option_up_menu_main();
  return true;
}
#endif

static inline gboolean is_statinfo_ndx(int ndx) {
  int type = pingelem[ndx].type; return ((type != PE_NO) && (type != PE_FILL));
}

static gboolean pt_init_line_elems(t_type_elem *elem, t_listline *line,
    t_listline *bodylines, GtkSizeGroup* group[PE_MAX]) {
  if (!line) return false;
  gboolean vis_n_dnd = (bodylines != NULL);
  for (int i = 0; i < PE_MAX; i++) {
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

static GtkWidget* pt_make_dynlist(t_listline *lines, int len, t_type_elem *elems,
    t_listline* bodylines, GtkSizeGroup* group[PE_MAX]) {
  static char stat_no_at_buff[MAXTTL][ELEM_BUFF_SIZE];
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(list, NULL);
  gtk_list_box_set_show_separators(GTK_LIST_BOX(list), true);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  t_type_elem bodyelem[G_N_ELEMENTS(pingelem)];
  memcpy(bodyelem, pingelem, sizeof(bodyelem)); // BUFFNOLINT
  for (int i = 0; i < len; i++) {
    GtkWidget *box = lines[i].box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_return_val_if_fail(box, NULL);
    if (style_loaded) gtk_widget_add_css_class(box, CSS_RPAD);
    gtk_widget_set_visible(box, elems != NULL);
    GtkListBoxRow *row = lines[i].row = line_row_new(box, elems != NULL);
    g_return_val_if_fail(row, NULL);
    if (elems) { // header
      if (!pt_init_line_elems(elems, &lines[i], bodylines, group)) return NULL;
    } else {     // line with number
      char *str = stat_no_at_buff[i]; g_snprintf(str, ELEM_BUFF_SIZE, "%d.", i + 1);
      bodyelem[PE_NO].name = str;
      if (!pt_init_line_elems(bodyelem, &lines[i], NULL, group)) return NULL;
    }
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
  }
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  return list;
}

static void pt_set_vis_cells(GtkWidget **labs) {
  if (labs) for (int i = 0; i < PE_MAX; i++) if (GTK_IS_WIDGET(labs[i]))
    gtk_widget_set_visible(labs[i], pingelem[i].enable);
}

static GtkWidget* pt_make_info(void) {
  pingtab.info.col = CSS_BGROUND;
  GtkWidget *list = gtk_list_box_new(); g_return_val_if_fail(list, NULL);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  GtkWidget *label = listbox.info.box = gtk_label_new(NULL); g_return_val_if_fail(label, NULL);
  gtk_widget_set_visible(label, false);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  listbox.info.row = line_row_new(label, false); g_return_val_if_fail(listbox.info.row, NULL);
  gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(listbox.info.row));
  return list;
}

static GtkWidget* pt_make_dyn(void) {
  GtkSizeGroup* group[PE_MAX];
  for (unsigned i = 0; i < G_N_ELEMENTS(group); i++) {
    group[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL); g_return_val_if_fail(group[i], NULL); }
  TAB_ELEM_WITH(pingtab.hdr, pt_make_dynlist(listbox.hdrlines, HDRLINES, pingelem, listbox.bodylines, group),
    NULL, CSS_BGROUND, NULL);
  gtk_box_append(GTK_BOX(pingtab.tab.w), pingtab.hdr.w);
  GtkWidget *dyn = pt_make_dynlist(listbox.bodylines, MAXTTL, NULL, NULL, group);
  for (unsigned i = 0; i < G_N_ELEMENTS(group); i++) if (group[i]) g_object_unref(group[i]);
  return dyn;
}


// pub
//

void pingtab_clear(void) {
  t_listline *line = listbox.bodylines;
  for (int i = 0; line && (i < MAXTTL); i++, line++) {
    if (!GTK_IS_WIDGET(line->row) || !GTK_IS_BOX(line->box)) continue;
    gtk_widget_set_visible(GTK_WIDGET(line->row), false);
    gtk_widget_set_visible(line->box, false);
    GtkWidget **lab = line->labs;
    for (int j = 0; *lab && (j < PE_MAX); j++, lab++) {
      if (GTK_IS_LABEL(*lab)) {
        gtk_widget_set_visible(*lab, false);
        if (is_statinfo_ndx(j)) gtk_label_set_text(GTK_LABEL(*lab), NULL); }}}
  gtk_widget_set_visible(GTK_WIDGET(listbox.info.row), false);
  gtk_widget_set_visible(listbox.info.box, false);
  gtk_label_set_label(GTK_LABEL(listbox.info.box), NULL);
}

void pingtab_update(void) {
  if (!pinger_state.pause) {
    t_listline *line = listbox.bodylines;
    for (int i = 0; line && (i < tgtat) && (i < MAXTTL); i++, line++) {
      GtkWidget **lab = line->labs;
      for (int j = 0; *lab && (j < PE_MAX); j++, lab++) {
        if (GTK_IS_LABEL(*lab) && is_statinfo_ndx(j) && pingelem[j].enable) {
          const char *text = stat_str_elem(i, pingelem[j].type);
          UPDATE_LABEL(*lab, text); }}}}
}

void pingtab_vis_rows(int upto) {
//  LOG("%s: %s %d", VISROWS_HDR, UPTO_HDR, upto);
  t_listline *line = listbox.bodylines;
  for (int i = 0; line && (i < MAXTTL); i++, line++) {
    gboolean vis = (i >= opts.range.min) && (i < upto);
    if (GTK_IS_WIDGET(line->row)) gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
    if (GTK_IS_WIDGET(line->box)) gtk_widget_set_visible(line->box, vis);
    GtkWidget **lab = line->labs;
    for (int j = 0; *lab && (j < PE_MAX); j++, lab++) if (GTK_IS_LABEL(*lab))
      gtk_widget_set_visible(*lab, vis && pingelem[j].enable);
  }
}

void pingtab_vis_cols(void) {
//  DEBUG("%s: %s", PING_HDR, VISROWS_HDR);
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
  pingtab.tag = PING_TAB_TAG;
  pingtab.tip = PING_TAB_TIP;
  if (!basetab_init(&pingtab, pt_make_dyn, pt_make_info)) return NULL;
  if (!clipboard_init(win, &pingtab)) LOG("%s: %s: %s", ERROR_HDR, pingtab.name, CLIPBOARD_HDR);
  return &pingtab;
}

