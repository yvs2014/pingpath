
#include <time.h>
#include "aux.h"

const char *timestampit(void) {
  static char now_ts[32];
  time_t now = time(NULL);
  strftime(now_ts, sizeof(now_ts), "%F %T", localtime(&now));
  return now_ts;
}

GtkListBoxRow* line_row_new(GtkWidget *child, bool visible) {
  GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
  g_return_val_if_fail(GTK_IS_LIST_BOX_ROW(row), NULL);
  gtk_list_box_row_set_child(row, child);
  gtk_widget_set_visible(GTK_WIDGET(row), visible);
  return row;
}

