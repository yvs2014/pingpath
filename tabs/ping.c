
#include "ping.h"
#include "pinger.h"
#include "stat.h"
#include "common.h"
#include "ui/styles.h"


#define ELEM_BUFF_SIZE 16
#define HOPNO_MAX_CHARS 3

#define HDRLINES 1

typedef struct listline {
  GtkListBoxRow* row;          // row from ListBox
  GtkWidget* child;            // child
  GtkWidget* cells[MAX_ELEMS]; // cells inside of child
} t_listline;

typedef struct listbox {
  t_listline header[HDRLINES];
  t_listline lines[MAXTTL];
  GtkWidget* info;
} t_listbox;

typedef struct area {
  GtkWidget *tab, *hdr, *dyn;
} t_area;

const t_elem_array elem_hdr = {
  [NDX_HOST] = "Host", [NDX_STAT] = "Stat",
};

t_area area;
t_listbox listbox;

static void align_elem_label(GtkWidget* label, int max, GtkAlign align, bool expand) {
  gtk_label_set_width_chars(GTK_LABEL(label), max);
  gtk_widget_set_halign(label, align);
  gtk_label_set_xalign(GTK_LABEL(label), align == GTK_ALIGN_END);
  gtk_widget_set_hexpand(label, expand);
}

static void set_elem_align(int ndx, GtkWidget *label) {
  switch (ndx) {
    case NDX_NO:
      align_elem_label(label, HOPNO_MAX_CHARS, GTK_ALIGN_END, false);
      break;
    case NDX_HOST:
      align_elem_label(label, ping_opts.dns ? host_name_max : host_addr_max, GTK_ALIGN_START, true);
      break;
    case NDX_STAT:
      align_elem_label(label, stat_all_max, GTK_ALIGN_START, false);
      break;
  }
  gtk_widget_set_valign(label, GTK_ALIGN_START);
}

static void init_child_elem(const t_elem_array str, t_listline *line, bool visible) {
  for (int j = 0; j < MAX_ELEMS; j++) {
    GtkWidget *elem = line->cells[j] = gtk_label_new(str[j]);
    g_assert(elem);
    set_elem_align(j, elem);
    gtk_widget_set_visible(elem, visible);
    gtk_box_append(GTK_BOX(line->child), elem);
  }
}

static GtkListBoxRow* line_row_new(GtkWidget *child, bool visible) {
  GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
  g_assert(row);
  gtk_list_box_row_set_child(row, child);
  gtk_widget_set_visible(GTK_WIDGET(row), visible);
  gtk_widget_set_visible(child, visible);
  return row;
}

static GtkWidget* init_list_box(t_listline *boxes, int boxes_len, bool vis, bool hdr) {
  static char stat_no_at_buff[MAXTTL][ELEM_BUFF_SIZE];
  GtkWidget *list = gtk_list_box_new();
  g_assert(list);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
  gtk_list_box_set_show_separators(GTK_LIST_BOX(list), true);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  for (int i = 0; i < boxes_len; i++) {
    GtkWidget *c = boxes[i].child = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_assert(c);
    gtk_widget_set_visible(c, vis);
    GtkListBoxRow *row = boxes[i].row = line_row_new(c, vis);
    g_assert(row);
    gtk_list_box_row_set_activatable(row, !hdr);
    const t_elem_array *arr;
    if (hdr) arr = &elem_hdr; else {
      char *s = stat_no_at_buff[i];
      snprintf(s, ELEM_BUFF_SIZE, "%d.", i + 1);
      const t_elem_array str = { [NDX_NO] = s };
      arr = &str;
    }
    init_child_elem(*arr, &boxes[i], vis);
    gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
  }
  return list;
}

static GtkWidget* init_info(void) {
  GtkWidget *label = listbox.info = gtk_label_new(NULL);
  g_assert(label);
  gtk_widget_set_visible(label, false);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  return label;
}

// pub
//
void clear_dynarea(void) {
  for (int i = 0; i < MAXTTL; i++) {
    t_listline *line = &listbox.lines[i];
    gtk_widget_set_visible(GTK_WIDGET(line->row), false);
    gtk_widget_set_visible(line->child, false);
    for (int j = 0; j < MAX_ELEMS; j++) {
      gtk_widget_set_visible(line->cells[j], false);
      if (j != NDX_NO) gtk_label_set_text(GTK_LABEL(line->cells[j]), NULL);
    }
  }
  gtk_widget_set_visible(listbox.info, false);
  gtk_label_set_label(GTK_LABEL(listbox.info), NULL);
  free_ping_errors();
}

gboolean update_dynarea(gpointer data) {
  if (!ping_opts.pause)
    for (int i = 0; i < hops_no; i++)
      for (int j = 0; j < MAX_ELEMS; j++)
        if (elem_hdr[j])
          gtk_label_set_text(GTK_LABEL(listbox.lines[i].cells[j]), stat_elem(i, j));
  return true;
}

void set_visible_lines(int no) {
  for (int i = 0; i < MAXTTL; i++) {
    bool vis = i < no;
    t_listline *line = &listbox.lines[i];
    for (int j = 0; j < MAX_ELEMS; j++) {
      gtk_widget_set_visible(GTK_WIDGET(line->row), vis);
      gtk_widget_set_visible(line->child, vis);
      gtk_widget_set_visible(line->cells[j], vis);
    }
  }
}

void update_elem_width(int max, int ndx) {
  for (int i = 0; i < HDRLINES; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.header[i].cells[ndx]), max);
  for (int i = 0; i < MAXTTL; i++)
    gtk_label_set_width_chars(GTK_LABEL(listbox.lines[i].cells[ndx]), max);
}

void set_errline(const gchar *s) {
  gtk_label_set_label(GTK_LABEL(listbox.info), s);
  gtk_widget_set_visible(listbox.info, true);
}

GtkWidget* init_pingtab() {
  area.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);                                                
  gtk_widget_set_name(area.tab, CSS_ID_PINGTAB);
  area.hdr = init_list_box(listbox.header, HDRLINES, true, true);
  gtk_widget_set_name(area.hdr, CSS_ID_HDRAREA);
  gtk_box_append(GTK_BOX(area.tab), area.hdr);
  area.dyn = init_list_box(listbox.lines, MAXTTL, false, false);
  gtk_widget_set_name(area.dyn, CSS_ID_DYNAREA);
  //
  GtkWidget *dynbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_assert(dynbox);
  gtk_box_append(GTK_BOX(dynbox), area.dyn);
  gtk_box_append(GTK_BOX(dynbox), init_info());
  //
  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), dynbox);
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(area.tab), scroll);
//gtk_widget_set_name(area.tab, CSS_IDG);
//gtk_widget_set_name(scroll, CSS_IDG);
  return area.tab;
}

