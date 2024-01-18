
#include "ping.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/clipboard.h"

#define ELEM_BUFF_SIZE 16
#define HOPNO_MAX_CHARS 3

#define HDRLINES 1

typedef struct listline {
  GtkListBoxRow *row;         // row from ListBox
  GtkWidget *child;           // child
  GtkWidget* cells[ELEM_MAX]; // cells inside of child
} t_listline;

typedef struct listbox {
  t_listline header[HDRLINES];
  t_listline lines[MAXTTL];
  t_listline info;
} t_listbox;

const gchar *info_mesg;
const gchar *notyet_mesg = "No data yet";

static t_listbox listbox;

static t_tab pingtab = { .self = &pingtab, .name = "ping-tab", .ico = PING_TAB_ICON, .tag = PING_TAB_TAG, .tip = PING_TAB_TIP,
  .desc = { [POP_MENU_NDX_COPY] = { .name = "win.ping_menu_copy" }, [POP_MENU_NDX_SALL] = { .name = "win.ping_menu_sall" }},
  .act = { [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l2 }, [POP_MENU_NDX_SALL] = { .activate = cb_on_sall }},
};

static void align_elem_label(GtkWidget* label, int max, GtkAlign align, gboolean expand) {
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
    case ELEM_AS:
    case ELEM_CC:
    case ELEM_DESC:
    case ELEM_RT:
      align_elem_label(label, stat_elem_max(typ), GTK_ALIGN_START, false);
      break;
    case ELEM_FILL:
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

static gboolean init_child_elem(const t_stat_elem *elem, t_listline *line, gboolean visible) {
  for (int i = 0; i < ELEM_MAX; i++) {
    GtkWidget *label = line->cells[i] = gtk_label_new(elem[i].name);
    g_return_val_if_fail(GTK_IS_LABEL(label), false);
    set_elem_align(i, label);
    gtk_widget_set_visible(label, visible && elem[i].enable);
    gtk_widget_set_tooltip_text(label, elem[i].tip);
    gtk_box_append(GTK_BOX(line->child), label);
  }
  return true;
}

static GtkWidget* init_list_box(t_listline *lines, int len, gboolean vis, gboolean hdr) {
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
    t_stat_elem *arr;
    if (hdr) arr = statelem; else {
      char *s = stat_no_at_buff[i];
      g_snprintf(s, ELEM_BUFF_SIZE, "%d.", i + 1);
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
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), NULL);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_MULTIPLE);
  gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), false);
  GtkWidget *label = listbox.info.child = gtk_label_new(NULL);
  g_return_val_if_fail(GTK_IS_LABEL(label), NULL);
  gtk_widget_set_visible(label, false);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  listbox.info.row = line_row_new(label, false);
  g_return_val_if_fail(GTK_IS_LIST_BOX_ROW(listbox.info.row), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(listbox.info.row));
  listbox.info.child = label;
  return list;
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
  gtk_widget_set_visible(GTK_WIDGET(listbox.info.row), false);
  gtk_widget_set_visible(listbox.info.child, false);
  gtk_label_set_label(GTK_LABEL(listbox.info.child), NULL);
  pinger_free_errors();
}

void pingtab_update(void) {
  static const gchar *nopong_mesg[] = { "Not reached", "Not reached yet"};
  if (!pinger_state.pause)
    for (int i = 0; i < hops_no; i++)
      for (int typ = 0; typ < ELEM_MAX; typ++)
        if ((typ != ELEM_NO) && statelem[typ].enable) {
          GtkWidget *label = listbox.lines[i].cells[typ];
          if (GTK_IS_LABEL(label)) {
            const gchar *text = stat_str_elem(i, typ);
            UPDATE_LABEL(label, text);
          }
        }
  { // no data display
    gboolean notyet = (info_mesg == notyet_mesg);
    if (pinger_state.gotdata) { if (notyet) pingtab_set_error(NULL); }
    else if (!notyet && !info_mesg && pinger_state.gotdata) pingtab_set_error(notyet_mesg);
  }
  if (pinger_state.gotdata && !pinger_state.reachable) {
    gboolean yet = pinger_state.run;
    if (!info_mesg || (!yet && (info_mesg == nopong_mesg[1])))
      pingtab_set_error(nopong_mesg[yet]);
  }
}

void pingtab_vis_rows(int no) {
  LOG("set upto %d visible rows", no);
  for (int i = 0; i < MAXTTL; i++) {
    gboolean vis = (i >= opts.min) && (i < no);
    t_listline *line = &listbox.lines[i];
    gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
    gtk_widget_set_visible(line->child, vis);
    for (int j = 0; j < ELEM_MAX; j++) gtk_widget_set_visible(line->cells[j], vis && statelem[j].enable);
  }
}

void pingtab_vis_cols(void) {
  DEBUG("set %s", "visible columns");
  for (int i = 0; i < MAXTTL; i++) set_vis_cells(&listbox.header[i]);
  for (int i = 0; i < MAXTTL; i++) set_vis_cells(&listbox.lines[i]);
  pingtab_update();
}

void pingtab_update_width(int typ, int max) {
  for (int i = 0; i < HDRLINES; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.header[i].cells[typ]), max);
  for (int i = 0; i < MAXTTL; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.lines[i].cells[typ]), max);
}

void pingtab_set_error(const gchar *error) {
  info_mesg = error;
  gtk_label_set_label(GTK_LABEL(listbox.info.child), error);
  gboolean vis = error && error[0];
  gtk_widget_set_visible(GTK_WIDGET(listbox.info.row), vis);
  gtk_widget_set_visible(listbox.info.child, vis);
}

t_tab* pingtab_init(GtkWidget* win) {
  pingtab.lab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  g_return_val_if_fail(GTK_IS_BOX(pingtab.lab), NULL);
  pingtab.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(pingtab.tab), NULL);
  pingtab.hdr = init_list_box(listbox.header, HDRLINES, true, true);
  gtk_box_append(GTK_BOX(pingtab.tab), pingtab.hdr);
  pingtab.dyn = init_list_box(listbox.lines, MAXTTL, false, false);
  g_return_val_if_fail(GTK_IS_LIST_BOX(pingtab.dyn), NULL);
  //
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  gtk_box_append(GTK_BOX(box), pingtab.dyn);
  pingtab.info = init_info();
  g_return_val_if_fail(GTK_IS_WIDGET(pingtab.info), NULL);
  gtk_box_append(GTK_BOX(box), pingtab.info);
  //
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(GTK_IS_SCROLLED_WINDOW(scroll), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(pingtab.tab), scroll);
  if (!clipboard_init(win, &pingtab)) LOG("no %s clipboard", pingtab.name);
  return &pingtab;
}

