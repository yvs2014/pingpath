
#include <time.h>

#include "common.h"
#include "ui/style.h"

const char *appver = APPNAME "-" VERSION;

const char *unkn_error = "unknown error";
const char *unkn_field = ""; // "?" "???"
const char *unkn_whois = "";

gboolean cli;
int verbose, start_page = TAB_PING_NDX;
t_tab* nb_tabs[TAB_NDX_MAX]; // notebook tabs are reorderable

t_type_elem pingelem[ELEM_MAX] = {
  [ELEM_NO]   = { .type = ELEM_NO,   .enable = true, .name = "" },
  [ELEM_HOST] = { .type = ELEM_HOST, .enable = true, .name = ELEM_HOST_HDR, .tip = ELEM_HOST_TIP },
  [ELEM_AS]   = { .type = ELEM_AS,   .enable = true, .name = ELEM_AS_HDR,   .tip = ELEM_AS_TIP   },
  [ELEM_CC]   = { .type = ELEM_CC,   .enable = true, .name = ELEM_CC_HDR,   .tip = ELEM_CC_TIP   },
  [ELEM_DESC] = { .type = ELEM_DESC,                 .name = ELEM_DESC_HDR, .tip = ELEM_DESC_TIP },
  [ELEM_RT]   = { .type = ELEM_RT,                   .name = ELEM_RT_HDR,   .tip = ELEM_RT_TIP   },
  [ELEM_FILL] = { .type = ELEM_FILL, .enable = true, .name = "" },
  [ELEM_LOSS] = { .type = ELEM_LOSS, .enable = true, .name = ELEM_LOSS_HDR, .tip = ELEM_LOSS_TIP },
  [ELEM_SENT] = { .type = ELEM_SENT, .enable = true, .name = ELEM_SENT_HDR, .tip = ELEM_SENT_TIP },
  [ELEM_RECV] = { .type = ELEM_RECV,                 .name = ELEM_RECV_HDR, .tip = ELEM_RECV_TIP },
  [ELEM_LAST] = { .type = ELEM_LAST, .enable = true, .name = ELEM_LAST_HDR, .tip = ELEM_LAST_TIP },
  [ELEM_BEST] = { .type = ELEM_BEST, .enable = true, .name = ELEM_BEST_HDR, .tip = ELEM_BEST_TIP },
  [ELEM_WRST] = { .type = ELEM_WRST, .enable = true, .name = ELEM_WRST_HDR, .tip = ELEM_WRST_TIP },
  [ELEM_AVRG] = { .type = ELEM_AVRG, .enable = true, .name = ELEM_AVRG_HDR, .tip = ELEM_AVRG_TIP },
  [ELEM_JTTR] = { .type = ELEM_JTTR, .enable = true, .name = ELEM_JTTR_HDR, .tip = ELEM_JTTR_TIP },
};

t_type_elem graphelem[GRGR_MAX] = {
  [GRLG_NO]   = { .type = GRLG_NO,   .enable = true, .name = "" },
  [GRLG_DASH] = { .type = GRLG_DASH, .enable = true, .name = GRLG_DASH_HEADER },
  [GRLG_AVJT] = { .type = GRLG_AVJT, .enable = true, .name = GRLG_AVJT_HEADER },
  [GRLG_CCAS] = { .type = GRLG_CCAS, .enable = true, .name = GRLG_CCAS_HEADER },
  [GRLG_LGHN] = { .type = GRLG_LGHN, .enable = true, .name = GRLG_LGHN_HDR    },
  [GREX_MEAN] = { .type = GREX_MEAN,                 .name = GREX_MEAN_HDR    },
  [GREX_JRNG] = { .type = GREX_JRNG,                 .name = GREX_JRNG_HDR    },
  [GREX_AREA] = { .type = GREX_AREA,                 .name = GREX_AREA_HDR    },
};

#ifdef WITH_PLOT
t_type_elem plotelem[PLEL_MAX] = {
  [PLEL_BACK] = { .type = PLEL_BACK, .enable = true, .name = PLEL_BACK_HDR },
  [PLEL_AXIS] = { .type = PLEL_AXIS, .enable = true, .name = PLEL_AXIS_HDR },
  [PLEL_GRID] = { .type = PLEL_GRID, .enable = true, .name = PLEL_GRID_HDR },
};
#endif

const double colors[][3] = { // 5x6 is enough for MAXTTL=30
  {1, 0, 0},     {0, 1, 0},     {0, 0, 1},     {1, 1, 0},         {1, 0, 1},         {0, 1, 1},
  {0.5, 0, 0},   {0, 0.5, 0},   {0, 0, 0.5},   {0.5, 0.5, 0},     {0.5, 0, 0.5},     {0, 0.5, 0.5},
  {0.75, 0, 0},  {0, 0.75, 0},  {0, 0, 0.75},  {0.75, 0.75, 0},   {0.75, 0, 0.75},   {0, 0.75, 0.75},
  {0.25, 0, 0},  {0, 0.25, 0},  {0, 0, 0.25},  {0.25, 0.25, 0},   {0.25, 0, 0.25},   {0, 0.25, 0.25},
  {0.875, 0, 0}, {0, 0.875, 0}, {0, 0, 0.875}, {0.875, 0.875, 0}, {0.875, 0, 0.875}, {0, 0.875, 0.875},
};

const int n_colors = G_N_ELEMENTS(colors);

//

static int elem_type2ndx(int type, t_type_elem *elem, int max) {
  for (int i = 0; i < max; i++) if (type == elem[i].type) return i;
  return -1;
}
int  pingelem_type2ndx(int type) { return elem_type2ndx(type, pingelem,  G_N_ELEMENTS(pingelem));  }
int graphelem_type2ndx(int type) { return elem_type2ndx(type, graphelem, G_N_ELEMENTS(graphelem)); }
#ifdef WITH_PLOT
int  plotelem_type2ndx(int type) { return elem_type2ndx(type, plotelem,  G_N_ELEMENTS(plotelem));  }
#endif

static const char* char_pattern[] = { [INFO_CHAR] = INFO_PATT, [STAT_CHAR] = STAT_PATT,
  [GRLG_CHAR] = GRLG_PATT, [GREX_CHAR] = GREX_PATT,
#ifdef WITH_PLOT
  [PLEL_CHAR] = PLEL_PATT,
#endif
};

#define MAXCHARS_IN_PATTERN 8
static int char_ndxs[][MAXCHARS_IN_PATTERN][2] = { // max pattern is 8 chars
  [INFO_CHAR] = {
    {ENT_BOOL_HOST, ELEM_HOST}, // h
    {ENT_BOOL_AS,   ELEM_AS},   // a
    {ENT_BOOL_CC,   ELEM_CC},   // c
    {ENT_BOOL_DESC, ELEM_DESC}, // d
    {ENT_BOOL_RT,   ELEM_RT},   // r
  },
  [STAT_CHAR] = {
    {ENT_BOOL_LOSS, ELEM_LOSS}, // l
    {ENT_BOOL_SENT, ELEM_SENT}, // s
    {ENT_BOOL_RECV, ELEM_RECV}, // r
    {ENT_BOOL_LAST, ELEM_LAST}, // m
    {ENT_BOOL_BEST, ELEM_BEST}, // b
    {ENT_BOOL_WRST, ELEM_WRST}, // w
    {ENT_BOOL_AVRG, ELEM_AVRG}, // a
    {ENT_BOOL_JTTR, ELEM_JTTR}, // j
  },
  [GRLG_CHAR] = {
    {ENT_BOOL_AVJT, GRLG_AVJT}, // d
    {ENT_BOOL_CCAS, GRLG_CCAS}, // c
    {ENT_BOOL_LGHN, GRLG_LGHN}, // h
  },
  [GREX_CHAR] = {
    {ENT_BOOL_MEAN, GREX_MEAN}, // l
    {ENT_BOOL_JRNG, GREX_JRNG}, // r
    {ENT_BOOL_AREA, GREX_AREA}, // a
  },
#ifdef WITH_PLOT
  [PLEL_CHAR] = {
    {ENT_BOOL_PLBK, PLEL_BACK}, // b
    {ENT_BOOL_PLAX, PLEL_AXIS}, // a
    {ENT_BOOL_PLGR, PLEL_GRID}, // g
  },
#endif
};

int char2ndx(int cat, gboolean ent, char c) {
  static int cat_no = G_N_ELEMENTS(char_ndxs);
  int n = -1;
  if ((cat >= 0) && (cat < cat_no)) switch (cat) {
    case INFO_CHAR:
    case STAT_CHAR:
    case GRLG_CHAR:
    case GREX_CHAR:
#ifdef WITH_PLOT
    case PLEL_CHAR:
#endif
    { char *p = strchr(char_pattern[cat], c);
      if (p) {
        int pos = p - char_pattern[cat];
        if (pos < MAXCHARS_IN_PATTERN) n = char_ndxs[cat][pos][ent ? 0 : 1];
      }
    } break;
  }
  return n;
}

static int pingelem_type2ent(int type) {
  int n = -1;
  switch (type) {
    case ELEM_HOST: n = ENT_BOOL_HOST; break;
    case ELEM_AS:   n = ENT_BOOL_AS;   break;
    case ELEM_CC:   n = ENT_BOOL_CC;   break;
    case ELEM_DESC: n = ENT_BOOL_DESC; break;
    case ELEM_RT:   n = ENT_BOOL_RT;   break;
    case ELEM_LOSS: n = ENT_BOOL_LOSS; break;
    case ELEM_SENT: n = ENT_BOOL_SENT; break;
    case ELEM_RECV: n = ENT_BOOL_RECV; break;
    case ELEM_LAST: n = ENT_BOOL_LAST; break;
    case ELEM_BEST: n = ENT_BOOL_BEST; break;
    case ELEM_WRST: n = ENT_BOOL_WRST; break;
    case ELEM_AVRG: n = ENT_BOOL_AVRG; break;
    case ELEM_JTTR: n = ENT_BOOL_JTTR; break;
  }
  return n;
}

static int graphelem_type2ent(int type) {
  int n = -1;
  switch (type) {
    case GRLG_AVJT: n = ENT_BOOL_AVJT; break;
    case GRLG_CCAS: n = ENT_BOOL_CCAS; break;
    case GRLG_LGHN: n = ENT_BOOL_LGHN; break;
    case GREX_MEAN: n = ENT_BOOL_MEAN; break;
    case GREX_JRNG: n = ENT_BOOL_JRNG; break;
    case GREX_AREA: n = ENT_BOOL_AREA; break;
  }
  return n;
}

#ifdef WITH_PLOT
static int plotelem_type2ent(int type) {
  int n = -1;
  switch (type) {
    case PLEL_BACK: n = ENT_BOOL_PLBK; break;
    case PLEL_AXIS: n = ENT_BOOL_PLAX; break;
    case PLEL_GRID: n = ENT_BOOL_PLGR; break;
  }
  return n;
}
#endif

t_elem_desc info_desc = { .elems = pingelem, .mm = { .min = ELEM_HOST, .max = ELEM_RT },
  .cat = INFO_CHAR, .patt = INFO_PATT, .t2n = pingelem_type2ndx, .t2e = pingelem_type2ent };
t_elem_desc stat_desc = { .elems = pingelem, .mm = { .min = ELEM_LOSS, .max = ELEM_JTTR },
  .cat = STAT_CHAR, .patt = STAT_PATT, .t2n = pingelem_type2ndx, .t2e = pingelem_type2ent };

t_elem_desc grlg_desc = { .elems = graphelem, .mm = { .min = GRLG_AVJT, .max = GRLG_LGHN },
  .cat = GRLG_CHAR, .patt = GRLG_PATT, .t2n = graphelem_type2ndx, .t2e = graphelem_type2ent };
t_elem_desc grex_desc = { .elems = graphelem, .mm = { .min = GREX_MEAN, .max = GREX_AREA },
  .cat = GREX_CHAR, .patt = GREX_PATT, .t2n = graphelem_type2ndx, .t2e = graphelem_type2ent };

#ifdef WITH_PLOT
t_elem_desc plot_desc = { .elems = plotelem, .mm = { .min = PLEL_BACK, .max = PLEL_GRID },
  .cat = PLEL_CHAR, .patt = PLEL_PATT, .t2n = plotelem_type2ndx, .t2e = plotelem_type2ent };
#endif

//

static unsigned rgb2x(double c) { int n = c * 255; return n % 256; }

inline const char* onoff(gboolean on) { return on ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR; }

char* get_nth_color(int i) {
  int n = i % n_colors;
  return g_strdup_printf("#%02x%02x%02x", rgb2x(colors[n][0]), rgb2x(colors[n][1]), rgb2x(colors[n][2]));
}

static gboolean* elem_enabler(int type, t_type_elem *elem, int max) {
  for (int i = 0; i < max; i++) if (type == elem[i].type) return &elem[i].enable;
  return NULL;
}
gboolean*  pingelem_enabler(int type) { return elem_enabler(type, pingelem,  G_N_ELEMENTS(pingelem));  }
gboolean* graphelem_enabler(int type) { return elem_enabler(type, graphelem, G_N_ELEMENTS(graphelem)); }

gboolean is_grelem_enabled(int type) {
  int ndx = graphelem_type2ndx(type);
  return ((ndx >= 0) && (ndx < GRGR_MAX)) ? graphelem[ndx].enable : false;
}

#ifdef WITH_PLOT
gboolean* plotelem_enabler(int type) { return elem_enabler(type, plotelem, G_N_ELEMENTS(plotelem)); }

gboolean is_plelem_enabled(int type) {
  int ndx = plotelem_type2ndx(type);
  return ((ndx >= 0) && (ndx < PLEL_MAX)) ? plotelem[ndx].enable : false;
}
#endif

#define CLEAN_ELEM_LOOP(elems, min, max) { for (int i = 0; i < G_N_ELEMENTS(elems); i++) \
  if ((min <= elems[i].type) && (elems[i].type <= max)) elems[i].enable = false; }

void clean_elems(int type) {
  switch (type) {
    case ENT_EXP_INFO: CLEAN_ELEM_LOOP(pingelem,  ELEM_HOST, ELEM_RT);   break;
    case ENT_EXP_STAT: CLEAN_ELEM_LOOP(pingelem,  ELEM_LOSS, ELEM_JTTR); break;
    case ENT_EXP_LGFL: CLEAN_ELEM_LOOP(graphelem, GRLG_AVJT, GRLG_LGHN); break;
    case ENT_EXP_GREX: CLEAN_ELEM_LOOP(graphelem, GREX_MEAN, GREX_AREA); break;
#ifdef WITH_PLOT
    case ENT_EXP_PLEL: CLEAN_ELEM_LOOP(plotelem,  PLEL_BACK, PLEL_GRID); break;
#endif
  }
}

const char *timestampit(void) {
  static char now_ts[32];
  time_t now = time(NULL);
  strftime(now_ts, sizeof(now_ts), "%F %T", localtime(&now));
  return now_ts;
}

char* rtver(int ndx) {
  int ver = 0;
  switch (ndx) {
    case GLIB_STRV: return
      g_strdup_printf("%d.%d.%d", glib_major_version, glib_minor_version, glib_micro_version);
    case GTK_STRV: return
      g_strdup_printf("%d.%d.%d", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    case CAIRO_STRV: ver = cairo_version(); break;
    case PANGO_STRV: ver = pango_version(); break;
  }
  return ver ? g_strdup_printf("%d.%d.%d", ver / 10000, (ver % 10000) / 100, ver % 100) : NULL;
}

GtkListBoxRow* line_row_new(GtkWidget *child, gboolean visible) {
  GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
  g_return_val_if_fail(row, NULL);
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
      gtk_box_append(GTK_BOX(tab->lab.w), gtk_image_new_from_icon_name(ico));
      if (tab->tag) gtk_box_append(GTK_BOX(tab->lab.w), gtk_label_new(tab->tag));
      if (tab->tip) gtk_widget_set_tooltip_text(tab->lab.w, tab->tip);
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
void print_refs(GSList *refs, const char *prefix) {
  if (verbose < 3) return;
  int i = 0;
  for (GSList* p = refs; p; p = p->next, i++) {
    t_ref *r = p->data; if (!r) continue;
    t_host *h = &r->hop->host[r->ndx];
    LOG("%sref[%d]: hop=%p: ndx=%d: addr=%s name=%s", prefix ? prefix : "", i, r->hop, r->ndx, h->addr, h->name);
  }
}
#endif

GSList* list_add_nodup(GSList **list, void *data, GCompareFunc cmp, int max) {
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

