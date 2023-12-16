
#include <time.h>
#include "common.h"

#ifdef LOGGING
const char *log_empty  = "<empty>";
#endif

const char *unkn_error = "unknown error";
const char *unkn_field = ""; // "?" "???" (see notes)
const char *unkn_whois = "";

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

void host_free(t_host *host) {
  if (host) { UPD_STR(host->addr, NULL); UPD_STR(host->name, NULL); }
}

int host_cmp(const void *a, const void *b) {
  if (!a || !b) return -1;
  return g_strcmp0(((t_host*)a)->addr, ((t_host*)b)->addr);
}

int ref_cmp(const t_ref *a, const t_ref *b) {
  if (!a || !b) return -1;
  return ((a->hop == b->hop) && (a->ndx == b->ndx)) ? 0 : 1;
}

t_ref* ref_new(t_hop *hop, int ndx) {
  t_ref *ref = g_malloc0(sizeof(t_ref));
  if (ref) { ref->hop = hop; ref->ndx = ndx; }
  return ref;
}

#if DNS_DEBUGGING || WHOIS_DEBUGGING
void print_refs(GSList *refs, const gchar *prefix) {
  int i = 0;
  for (GSList* p = refs; p; p = p->next, i++) {
    t_ref *r = p->data; if (!r) continue;
    t_host *h = &r->hop->host[r->ndx];
    LOG("%sref[%d]: hop=%p: ndx=%d: addr=%s name=%s", prefix ? prefix : "", i, r->hop, r->ndx, h->addr, h->name);
  }
}
#endif

GSList* list_add_nodup(GSList **list, gpointer data, GCompareFunc cmp, int max) {
  if (!list || !data || !cmp) return NULL;
  GSList *elem = g_slist_find_custom(*list, data, cmp);
  if (elem) return elem;
  *list = g_slist_prepend(*list, data);
  if ((max > 0) && (g_slist_length(*list) > max))
    *list = g_slist_delete_link(*list, g_slist_last(*list));
  return *list;
}

