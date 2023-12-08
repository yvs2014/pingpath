
#include "ping.h"
#include "pinger.h"
#include "stat.h"
#include "common.h"
#include "ui/style.h"

#define ELEM_BUFF_SIZE 16
#define HOPNO_MAX_CHARS 3

#define HDRLINES 1

typedef struct listline {
  GtkListBoxRow* row;         // row from ListBox
  GtkWidget* child;           // child
  GtkWidget* cells[ELEM_MAX]; // cells inside of child
} t_listline;

typedef struct listbox {
  t_listline header[HDRLINES];
  t_listline lines[MAXTTL];
  GtkWidget* info;
} t_listbox;

static t_area area;
static t_listbox listbox;

const gchar *info_mesg;
const gchar *notyet_mesg = "No data yet";

static void align_elem_label(GtkWidget* label, int max, GtkAlign align, bool expand) {
  gtk_label_set_width_chars(GTK_LABEL(label), max);
  gtk_widget_set_halign(label, align);
  gtk_label_set_xalign(GTK_LABEL(label), align == GTK_ALIGN_END);
  gtk_widget_set_hexpand(label, expand);
}

static void set_elem_align(int typ, GtkWidget *label) {
  switch (typ) {
    case ELEM_NO:
      align_elem_label(label, HOPNO_MAX_CHARS, GTK_ALIGN_END, false);
      break;
    case ELEM_HOST:
      align_elem_label(label, stat_elem_max(typ), GTK_ALIGN_START, true);
      break;
    case ELEM_LOSS:
    case ELEM_SENT:
    case ELEM_RECV:
    case ELEM_LAST:
    case ELEM_BEST:
    case ELEM_WRST:
    case ELEM_AVRG:
    case ELEM_JTTR:
      align_elem_label(label, stat_elem_max(typ), GTK_ALIGN_END, false);
      break;
  }
  gtk_widget_set_valign(label, GTK_ALIGN_START);
}

static bool init_child_elem(const t_stat_elem *str, t_listline *line, bool visible) {
  for (int i = 0; i < ELEM_MAX; i++) {
    GtkWidget *elem = line->cells[i] = gtk_label_new(str[i].name);
    g_return_val_if_fail(GTK_IS_LABEL(elem), false);
    set_elem_align(i, elem);
    gtk_widget_set_visible(elem, visible && str[i].enable);
    gtk_box_append(GTK_BOX(line->child), elem);
  }
  return true;
}

static GtkWidget* init_list_box(t_listline *lines, int len, bool vis, bool hdr) {
  static char stat_no_at_buff[MAXTTL][ELEM_BUFF_SIZE];
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), NULL);
  gtk_list_box_set_show_separators(GTK_LIST_BOX(list), true);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  for (int i = 0; i < len; i++) {
    GtkWidget *c = lines[i].child = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_return_val_if_fail(GTK_IS_BOX(c), NULL);
    if (style_loaded) gtk_widget_add_css_class(c, CSS_PAD6);
    gtk_widget_set_visible(c, vis);
    GtkListBoxRow *row = lines[i].row = line_row_new(c, vis);
    g_return_val_if_fail(GTK_IS_LIST_BOX_ROW(row), NULL);
    gtk_list_box_row_set_activatable(row, !hdr);
    t_stat_elem *arr;
    if (hdr) arr = statelem; else {
      char *s = stat_no_at_buff[i];
      snprintf(s, ELEM_BUFF_SIZE, "%d.", i + 1);
      t_stat_elem str[ELEM_MAX] = {[ELEM_NO] = { .enable = statelem[i].enable, .name = s }};
      arr = str;
    }
    if (!init_child_elem(arr, &lines[i], vis)) return NULL;
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
  }
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  return list;
}

static GtkWidget* init_info(void) {
  GtkWidget *label = listbox.info = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), NULL);
  gtk_widget_set_visible(label, false);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  return label;
}

static void set_vis_cells(t_listline *line) {
  if (line) for (int i = 0; i < ELEM_MAX; i++) gtk_widget_set_visible(line->cells[i], statelem[i].enable);
}


// pub
//
void pingtab_clear(void) {
  for (int i = 0; i < MAXTTL; i++) {
    t_listline *line = &listbox.lines[i];
    gtk_widget_set_visible(GTK_WIDGET(line->row), false);
    gtk_widget_set_visible(line->child, false);
    for (int j = 0; j < ELEM_MAX; j++) {
      gtk_widget_set_visible(line->cells[j], false);
      if (j != ELEM_NO) gtk_label_set_text(GTK_LABEL(line->cells[j]), NULL);
    }
  }
  gtk_widget_set_visible(listbox.info, false);
  gtk_label_set_label(GTK_LABEL(listbox.info), NULL);
  pinger_free_errors();
}

gboolean pingtab_update(gpointer data) {
  static const gchar *nopong_mesg = "No response";
  if (!pinger_state.pause)
    for (int i = 0; i < hops_no; i++)
      for (int j = 0; j < ELEM_MAX; j++)
        if ((j != ELEM_NO) && statelem[j].enable)
          gtk_label_set_text(GTK_LABEL(listbox.lines[i].cells[j]), stat_elem(i, j));
  { // no data display
    bool notyet = info_mesg == notyet_mesg;
    if (pinger_state.gotdata) { if (notyet) pingtab_set_error(NULL); }
    else { if (!notyet && !info_mesg) pingtab_set_error(notyet_mesg); }
  }
  if (!info_mesg && !pinger_state.reachable) stat_set_nopong(nopong_mesg); // no response display
  return true;
}

inline void pingtab_wrap_update(void) { pingtab_update(NULL); }

void pingtab_vis_rows(int no) {
  LOG("set upto %d visible rows", no);
  for (int i = 0; i < MAXTTL; i++) {
    bool vis = i < no;
    t_listline *line = &listbox.lines[i];
    gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
    gtk_widget_set_visible(line->child, vis);
    for (int j = 0; j < ELEM_MAX; j++) gtk_widget_set_visible(line->cells[j], vis && statelem[j].enable);
  }
}

void pingtab_vis_cols(void) {
  LOG("set %s", "visible columns");
  for (int i = 0; i < MAXTTL; i++) set_vis_cells(&listbox.header[i]);
  for (int i = 0; i < MAXTTL; i++) set_vis_cells(&listbox.lines[i]);
  pingtab_wrap_update();
}

void pingtab_update_width(int max, int ndx) {
  for (int i = 0; i < HDRLINES; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.header[i].cells[ndx]), max);
  for (int i = 0; i < MAXTTL; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.lines[i].cells[ndx]), max);
}

void pingtab_set_error(const gchar *error) {
  info_mesg = error;
  gtk_label_set_label(GTK_LABEL(listbox.info), error);
  gtk_widget_set_visible(listbox.info, true);
}

t_area* pingtab_init() {
  area.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(area.tab), NULL);
  if (style_loaded) gtk_widget_set_name(area.tab, CSS_ID_PINGTAB);
  area.hdr = init_list_box(listbox.header, HDRLINES, true, true);
  if (style_loaded) gtk_widget_add_css_class(area.hdr, CSS_BGROUND);
  gtk_box_append(GTK_BOX(area.tab), area.hdr);
  area.dyn = init_list_box(listbox.lines, MAXTTL, false, false);
  g_return_val_if_fail(GTK_IS_LIST_BOX(area.dyn), NULL);
  if (style_loaded) gtk_widget_add_css_class(area.dyn, CSS_BGROUND);
  //
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  gtk_box_append(GTK_BOX(box), area.dyn);
  GtkWidget *info = init_info();
  g_return_val_if_fail(GTK_IS_WIDGET(info), NULL);
  gtk_box_append(GTK_BOX(box), info);
  //
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(GTK_IS_SCROLLED_WINDOW(scroll), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(area.tab), scroll);
  return &area;
}

