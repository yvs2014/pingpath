
#include "log.h"
#include "common.h"

static t_tab logtab = { .ico = LOG_TAB_ICON, .tag = LOG_TAB_TAG };

// pub
//

t_tab* logtab_init(void) {
  logtab.lab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  g_return_val_if_fail(GTK_IS_BOX(logtab.lab), NULL);
  logtab.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
  g_return_val_if_fail(GTK_IS_BOX(logtab.tab), NULL);
  logtab.dyn = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(logtab.dyn), NULL);
GtkWidget *cnt = gtk_label_new("log content"); // TMP
gtk_list_box_append(GTK_LIST_BOX(logtab.dyn), cnt); // TMP
  gtk_box_append(GTK_BOX(logtab.tab), logtab.dyn);
  return &logtab;
}

