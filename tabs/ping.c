
#include <math.h>

#include "ping.h"
#include "common.h"
#include "aux.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/clipboard.h"
#include "ui/option.h"

#define VISIBLE true

enum { ELEM_BUFF_SIZE = 16, HDRLINES = 1, BODYLINES = MAXTTL, INFOLINES = 1 };

typedef struct listline t_listline;

typedef struct len_listline {
  uint len;
  t_listline *list;
} t_len_listline;

#ifdef WITH_DND
typedef struct pt_dnd {
  GtkWidget *w, *b;
  t_len_listline head, body;
  const t_elem_desc *desc;
  int dx, dy;
} t_pt_dnd;
#endif

typedef struct listline {
  GtkListBoxRow *row;
  GtkWidget *box;            // row child
  GtkWidget* labels[PE_MAX]; // cache of box labels
#ifdef WITH_DND
  t_pt_dnd dnd[PE_MAX];
#endif
} t_listline;

static t_listline _hlist[HDRLINES];
static t_len_listline hbox = {.len = G_N_ELEMENTS(_hlist), .list = _hlist};
static t_listline _blist[BODYLINES];
static t_len_listline bbox = {.len = G_N_ELEMENTS(_blist), .list = _blist};
static t_listline _ilist[INFOLINES];
static t_len_listline ibox = {.len = G_N_ELEMENTS(_ilist), .list = _ilist};

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

static gboolean pt_reorder_cells(t_len_listline lines, int sn, int dn, gboolean before) {
  gboolean okay = true;
  t_listline *list = lines.list;
  for (uint i = 0; list && (i < lines.len); i++, list++) { // reorder line elements
    GtkWidget *box = list->box;
    GtkWidget *slab = pt_box_nth_label(box, sn), *dlab = pt_box_nth_label(box, dn);
    if (GTK_IS_BOX(box) && GTK_IS_WIDGET(slab) && GTK_IS_WIDGET(dlab)) {
      gtk_box_reorder_child_after(GTK_BOX(box), slab, before ? gtk_widget_get_prev_sibling(dlab) : dlab);
      memset(list->labels, 0, sizeof(list->labels)); // BUFFNOLINT
      pt_recache_labels(box, list->labels);
      continue;
    }
    DEBUG("%s: %s: #%u: %d -> %d", ERROR_HDR, DEB_REORDER, i, sn, dn);
    if (okay)
      okay = false;
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
  if (!sdnd || !ddnd || !ddnd->desc ||
      !ddnd->head.list || !ddnd->head.len ||
      !ddnd->body.list || !ddnd->body.len)
    return false;
  int sn = pt_box_wndx(ddnd->b, sdnd->w), dn = pt_box_wndx(ddnd->b, ddnd->w);
  if ((sn < 0) || (dn < 0) || (sn == dn)) return false;
  if (!GTK_IS_WIDGET(sdnd->w) || !GTK_IS_WIDGET(ddnd->w) || (sdnd->w == ddnd->w)) return false;
  if (sdnd->desc != ddnd->desc) { DNDORD("%s: %s", DEB_DND, "different groups"); return false; }
  gboolean before = gtk_widget_contains(ddnd->w, x * 2, y);
  PT_PRINT_ELEMS(ARRAY_HDR, "<<<", ddnd->desc->elems, PE_MAX);
  PT_PRINT_ELEMS(GROUP_HDR, "<<<", &ddnd->desc->elems[ddnd->desc->mm.min], ddnd->desc->mm.max - ddnd->desc->mm.min + 1);
  if (pt_reorder_elems(sn, before ? dn : dn + 1, ddnd->desc)) { // reorder inplace asserting no further errors
    if (!pt_reorder_cells(ddnd->head, sn, dn, before))
      g_critical("%s: %s", DEB_REORDER, RDR_PART_ERR);
    if (!pt_reorder_cells(ddnd->body, sn, dn, before))
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

static inline gboolean is_statinfo_ndx(uint ndx) {
  int type = pingelem[ndx].type;
  return ((type != PE_NO) && (type != PE_FILL));
}

#ifdef WITH_DND
static void set_line_dnd(t_pt_dnd *dnd, t_listline *line, GtkWidget *widget, t_len_listline body,
  gboolean setdnd, t_elem_desc *desc)
{
  *dnd = (t_pt_dnd){
    .w = widget,
    .b = line->box,
    .head = (t_len_listline){.len = 1, .list = line},
    .body = body,
    .desc = desc,
  };
  if (setdnd) {
    GtkDragSource *dnd_src = gtk_drag_source_new();
    if (dnd_src) {
      g_signal_connect(dnd_src, EV_DND_DRAG, G_CALLBACK(pt_hdr_dnd_drag), dnd);
      g_signal_connect(dnd_src, EV_DND_ICON, G_CALLBACK(pt_hdr_dnd_icon), dnd);
      gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(dnd_src));
    }
    GtkDropTarget *dnd_dst = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_COPY);
    if (dnd_dst) {
      gtk_drop_target_set_preload(dnd_dst, true);
      g_signal_connect(dnd_dst, EV_DND_MOVE, G_CALLBACK(pt_hdr_on_move), dnd);
      g_signal_connect(dnd_dst, EV_DND_DROP, G_CALLBACK(pt_hdr_on_drop), dnd);
      gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(dnd_dst));
    }
  }
}
#endif

static gboolean pt_init_line_elems(t_type_elem *elem, t_listline *line,
  t_len_listline body G_GNUC_UNUSED, GtkSizeGroup* group[], uint glen)
{
  if (!line)
    return false;
  gboolean vis_n_dnd = (body.list != NULL);
  for (uint i = 0; i < G_N_ELEMENTS(line->labels); i++) {
    GtkWidget *label = line->labels[i] = gtk_label_new(elem[i].name);
    g_return_val_if_fail(label, false);
    if ((i < glen) && group[i])
      gtk_size_group_add_widget(group[i], label);
    pt_set_elem_align(pingelem[i].type, label);
    gtk_widget_set_visible(label, vis_n_dnd && elem[i].enable);
    gtk_widget_set_tooltip_text(label, elem[i].tip);
    gtk_box_append(GTK_BOX(line->box), label);
#ifdef WITH_DND
    set_line_dnd(&line->dnd[i], line, label, body, vis_n_dnd && is_statinfo_ndx(i),
      IS_INFO_NDX(i) ? &info_desc :
      IS_STAT_NDX(i) ? &stat_desc :
      NULL);
#endif
  }
  return true;
}

static GtkWidget* pt_make_dynlist(t_len_listline lines, gboolean visible, t_type_elem elems[], // NONNULL(3)
  GtkSizeGroup* group[], uint glen)
{
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(list, NULL);
  gtk_list_box_set_show_separators(GTK_LIST_BOX(list), true);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  t_len_listline body = visible ? bbox : (t_len_listline){0};
  for (uint i = 0; i < lines.len; i++) {
    GtkWidget *box = lines.list[i].box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_return_val_if_fail(box, NULL);
    if (style_loaded)
      gtk_widget_add_css_class(box, CSS_RPAD);
    gtk_widget_set_visible(box, visible);
    GtkListBoxRow *row = lines.list[i].row = line_row_new(box, visible);
    g_return_val_if_fail(row, NULL);
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
    //
    if (!pt_init_line_elems(elems, &lines.list[i], body, group, glen))
      return NULL;
  }
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  return list;
}

static void pt_set_vis_cells(GtkWidget **labs) {
  if (labs)
    for (uint i = 0; i < G_N_ELEMENTS(pingelem); i++)
      if (GTK_IS_WIDGET(labs[i]))
        gtk_widget_set_visible(labs[i], pingelem[i].enable);
}

static GtkWidget* pt_make_info(void) {
  pingtab.info.col = CSS_BGROUND;
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(list, NULL);
  t_listline *l = ibox.list;
  for (uint i = 0; l && (i < ibox.len); i++, l++) {
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
    l->box = gtk_label_new(NULL);
    if (!l->box)
      break;
    gtk_widget_set_visible(l->box, false);
    gtk_widget_set_halign(l->box, GTK_ALIGN_START);
    l->row = line_row_new(l->box, false);
    if (!l->row)
      break;
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(l->row));
  }
  return list;
}

static GtkWidget* pt_make_dyn(void) {
  GtkSizeGroup* group[PE_MAX] = {0};
  for (uint i = 0; i < G_N_ELEMENTS(group); i++) {
    group[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    g_return_val_if_fail(group[i], NULL);
  }
  //
  pingtab.hdr.css = NULL;
  pingtab.hdr.col = CSS_BGROUND;
  pingtab.hdr.w = pt_make_dynlist(hbox, true, pingelem, group, G_N_ELEMENTS(group));
  g_return_val_if_fail(pingtab.hdr.w, NULL);
  gtk_box_append(GTK_BOX(pingtab.tab.w), pingtab.hdr.w);
  //
  static char stat_no_at_buff[G_N_ELEMENTS(_blist)][ELEM_BUFF_SIZE];
  t_type_elem bodyline[G_N_ELEMENTS(pingelem)];
  memcpy(bodyline, pingelem, sizeof(bodyline)); // BUFFNOLINT
  for (uint i = 0; i < bbox.len; i++) {
    snprintg(stat_no_at_buff[i], sizeof(stat_no_at_buff[i]), "%d.", i + 1);
    bodyline[PE_NO].name = stat_no_at_buff[i];
  }
  GtkWidget *dyn = pt_make_dynlist(bbox, false, bodyline, group, G_N_ELEMENTS(group));
  //
  for (uint i = 0; i < G_N_ELEMENTS(group); i++)
    if (group[i])
      g_object_unref(group[i]);
  return dyn;
}

static void clear_bbox(void) {
  t_listline *l = bbox.list;
  for (uint i = 0; l && (i < bbox.len); i++, l++) {
    if (!GTK_IS_WIDGET(l->row) || !GTK_IS_BOX(l->box))
      continue;
    gtk_widget_set_visible(GTK_WIDGET(l->row), false);
    gtk_widget_set_visible(l->box, false);
    GtkWidget **label = l->labels;
    for (uint j = 0; *label && (j < G_N_ELEMENTS(l->labels)); j++, label++) {
      if (GTK_IS_LABEL(*label)) {
        gtk_widget_set_visible(*label, false);
        if (is_statinfo_ndx(j))
          gtk_label_set_text(GTK_LABEL(*label), NULL);
      }
    }
  }
}

static void clear_ibox(void) {
  t_listline *l = ibox.list;
  for (uint i = 0; i < ibox.len; i++, l++) {
    gtk_widget_set_visible(GTK_WIDGET(l->row), false);
    gtk_widget_set_visible(l->box, false);
    gtk_label_set_label(GTK_LABEL(l->box), NULL);
  }
}


// pub
//

void pingtab_clear(void) {
  clear_bbox();
  clear_ibox();
}

void pingtab_update(void) {
#define LABELEM_OK (*label && elem && (j < MIN(G_N_ELEMENTS(pingelem), G_N_ELEMENTS(line->labels))))
  if (!pinger_state.pause) {
    t_listline *line = bbox.list;
    for (int at = 0; line && (at < tgtat) && (at < (int)bbox.len); at++, line++) {
      GtkWidget **label = line->labels;
      t_type_elem *elem = pingelem;
      for (uint j = 0; LABELEM_OK; j++, label++, elem++)
        if (elem->enable && elem->view) {
          const char *text = elem->view(at);
          UPDATE_LABEL(*label, text);
        }
    }
  }
}

void pingtab_vis_rows(int upto) {
//  LOG("%s: %s %d", VISROWS_HDR, UPTO_HDR, upto);
  t_listline *line = bbox.list;
  for (int i = 0; line && ((uint)i < bbox.len); i++, line++) {
    gboolean vis = (i >= opts.range.min) && (i < upto);
    if (GTK_IS_WIDGET(line->row))
      gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
    if (GTK_IS_WIDGET(line->box))
      gtk_widget_set_visible(line->box, vis);
    GtkWidget **label = line->labels;
    t_type_elem *elem = pingelem;
    for (uint j = 0; *label && elem && (j < MIN(G_N_ELEMENTS(pingelem), G_N_ELEMENTS(line->labels))); j++, label++, elem++)
      if (GTK_IS_LABEL(*label))
        gtk_widget_set_visible(*label, vis && elem->enable);
  }
}

void pingtab_vis_cols(void) {
//  DEBUG("%s: %s", PING_HDR, VISROWS_HDR);
  for (uint i = 0; i < hbox.len; i++)
    pt_set_vis_cells(hbox.list[i].labels);
  for (uint i = 0; i < bbox.len; i++)
    pt_set_vis_cells(bbox.list[i].labels);
  pingtab_update();
}

void pingtab_set_error(const char *error) {
  gtk_label_set_label(GTK_LABEL(ibox.list[0].box), error);
  gboolean vis = error && error[0];
  gtk_widget_set_visible(GTK_WIDGET(ibox.list[0].row), vis);
  gtk_widget_set_visible(ibox.list[0].box, vis);
}

t_tab* pingtab_init(GtkWidget* win) {
  if (!pingtab.tag)
    pingtab.tag = PING_TAB_TAG;
  if (!pingtab.tip)
    pingtab.tip = PING_TAB_TIP;
  if (!basetab_init(&pingtab, pt_make_dyn, pt_make_info))
    return NULL;
  if (!clipboard_init(win, &pingtab))
    LOG("%s: %s: %s", ERROR_HDR, pingtab.name, CLIPBOARD_HDR);
  return &pingtab;
}

