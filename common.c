
#include <time.h>

#include "common.h"
#include "ui/style.h"

const char *appver = APPNAME "-" VERSION;

const char *unkn_error = "unknown error";
const char *unkn_field = ""; // "?" "???" (see notes)
const char *unkn_whois = "";

gboolean cli;
gint verbose, start_page = TAB_PING_NDX; // TAB_GRAPH_NDX;

const double colors[][3] = { // 5x6 is enough for MAXTTL=30
  {1, 0, 0},     {0, 1, 0},     {0, 0, 1},     {1, 1, 0},         {1, 0, 1},         {0, 1, 1},
  {0.5, 0, 0},   {0, 0.5, 0},   {0, 0, 0.5},   {0.5, 0.5, 0},     {0.5, 0, 0.5},     {0, 0.5, 0.5},
  {0.75, 0, 0},  {0, 0.75, 0},  {0, 0, 0.75},  {0.75, 0.75, 0},   {0.75, 0, 0.75},   {0, 0.75, 0.75},
  {0.25, 0, 0},  {0, 0.25, 0},  {0, 0, 0.25},  {0.25, 0.25, 0},   {0.25, 0, 0.25},   {0, 0.25, 0.25},
  {0.875, 0, 0}, {0, 0.875, 0}, {0, 0, 0.875}, {0.875, 0.875, 0}, {0.875, 0, 0.875}, {0, 0.875, 0.875},
};

const int n_colors = G_N_ELEMENTS(colors);

t_stat_elem graphelem[GREL_MAX] = {
  [GRLG_NO]   = { .enable = true, .name = "" },
  [GRLG_DASH] = { .enable = true, .name = GRLG_DASH_HEADER },
  [GRLG_AVJT] = { .enable = true, .name = GRLG_AVJT_HEADER },
  [GRLG_CCAS] = { .enable = true, .name = GRLG_CCAS_HEADER },
  [GRLG_LGHN] = { .enable = true, .name = GRLG_LGHN_HDR    },
  [GREL_MEAN] = {                 .name = GREX_MEAN_HDR    },
  [GREL_JRNG] = {                 .name = GREX_JRNG_HDR    },
  [GREL_AREA] = {                 .name = GREX_AREA_HDR    },
};

t_tab* nb_tabs[TAB_NDX_MAX]; // note: nb list is reorderable

//

static unsigned rgb2x(double c) { int n = c * 255; return n % 256; }

gchar* get_nth_color(int i) {
  int n = i % n_colors;
  return g_strdup_printf("#%02x%02x%02x", rgb2x(colors[n][0]), rgb2x(colors[n][1]), rgb2x(colors[n][2]));
}

const char *timestampit(void) {
  static char now_ts[32];
  time_t now = time(NULL);
  strftime(now_ts, sizeof(now_ts), "%F %T", localtime(&now));
  return now_ts;
}

gchar* rtver(int ndx) {
  int ver = 0;
  switch (ndx) {
    case GTK_STRV: return
      g_strdup_printf("%d.%d.%d", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    case CAIRO_STRV: ver = cairo_version(); break;
    case PANGO_STRV: ver = pango_version(); break;
  }
  return ver ? g_strdup_printf("%d.%d.%d", ver / 10000, (ver % 10000) / 100, ver % 100) : NULL;
}

GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible) {
  GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
  g_return_val_if_fail(GTK_IS_LIST_BOX_ROW(row), NULL);
  gtk_list_box_row_set_child(row, child);
  gtk_widget_set_visible(GTK_WIDGET(row), visible);
  return row;
}

void tab_setup(t_tab *tab) {
  if (!tab) return;
  if (GTK_IS_WIDGET(tab->lab.w)) {
    gtk_widget_set_hexpand(tab->lab.w, true);
    if (style_loaded && tab->lab.css) gtk_widget_add_css_class(tab->lab.w, tab->lab.css);
    if (GTK_IS_BOX(tab->lab.w)) {
      const char *ico = is_sysicon(tab->ico);
      if (!ico) WARN("No icon found for %s", tab->name);
      GtkWidget *image = gtk_image_new_from_icon_name(ico);
      if (GTK_IS_IMAGE(image)) gtk_box_append(GTK_BOX(tab->lab.w), image);
      if (tab->tag) {
        GtkWidget *tag = gtk_label_new(tab->tag);
        if (GTK_IS_LABEL(tag)) gtk_box_append(GTK_BOX(tab->lab.w), tag);
      }
      gtk_widget_set_tooltip_text(tab->lab.w, tab->tip);
    }
  }
  t_tab_widget tw[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tw); i++) if (GTK_IS_WIDGET(tw[i].w))
    gtk_widget_set_can_focus(tw[i].w, false);
  if (style_loaded && tab->tab.css && GTK_IS_WIDGET(tab->tab.w))
    gtk_widget_add_css_class(tab->tab.w, tab->tab.css);
}

#define RELOAD_TW_CSS(tw, reload) { const char *col = reload; \
  if ((tw).col) gtk_widget_remove_css_class((tw).w, (tw).col); \
  (tw).col = col; gtk_widget_add_css_class((tw).w, (tw).col); }

void tab_color(t_tab *tab) {
  if (!style_loaded || !tab) return;
  t_tab_widget tw[] = {tab->hdr, tab->dyn, tab->info};
  for (int i = 0; i < G_N_ELEMENTS(tw); i++) if (tw[i].col && GTK_IS_WIDGET(tw[i].w))
    RELOAD_TW_CSS(tw[i], tw[i].col);
  if (tab->tab.col && GTK_IS_WIDGET(tab->tab.w))
    RELOAD_TW_CSS(tab->tab, tab->tab.col);
}

void tab_reload_theme(void) {
  for (int i = 0; i < G_N_ELEMENTS(nb_tabs); i++) if (nb_tabs[i]) tab_color(nb_tabs[i]);
}

void host_free(t_host *host) {
  if (host) { CLR_STR(host->addr); CLR_STR(host->name); }
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
  if (verbose < 3) return;
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
  if (elem) { g_free(data); return elem; }
  *list = g_slist_prepend(*list, data);
  if ((max > 0) && (g_slist_length(*list) > max))
    *list = g_slist_delete_link(*list, g_slist_last(*list));
  return *list;
}

GSList* list_add_ref(GSList **list, t_hop *hop, int ndx) {
  t_ref *ref = ref_new(hop, ndx);
  if (!ref) return NULL;
  if (list) {
    GSList *elem = g_slist_find_custom(*list, ref, (GCompareFunc)ref_cmp);
    if (elem) { g_free(ref); return elem; }
  }
  *list = g_slist_prepend(*list, ref);
  if (g_slist_length(*list) > REF_MAX)
    *list = g_slist_delete_link(*list, g_slist_last(*list));
  return *list;
}

