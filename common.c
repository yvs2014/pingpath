
#include <time.h>

#include "common.h"

const char *appver = APPNAME "-" VERSION;

const char *unkn_error = "unknown error";
const char *unkn_field = ""; // "?" "???"
const char *unkn_whois = "";

gboolean cli;
int verbose, activetab = TAB_PING_NDX;

t_opts opts = { .target = NULL, .dns = DEF_DNS, .whois = DEF_WHOIS, .cycles = DEF_CYCLES, .qos = DEF_QOS, .size = DEF_PSIZE,
  .min = MINTTL - 1, .lim = MAXTTL, .timeout = DEF_TOUT, .tout_usec = DEF_TOUT * G_USEC_PER_SEC, .logmax = DEF_LOGMAX,
  .graph = GRAPH_TYPE_CURVE, .legend = DEF_LEGEND, .darktheme = DEF_DARK_MAIN, .darkgraph = DEF_DARK_GRAPH,
#ifdef WITH_PLOT
  .plot = true, .darkplot = DEF_DARK_PLOT,
  .rcol = {DEF_RCOL_FROM, DEF_RCOL_TO},
  .gcol = {DEF_GCOL_FROM, DEF_GCOL_TO},
  .bcol = {DEF_BCOL_FROM, DEF_BCOL_TO},
  .rglob   = DEF_RGLOBAL,
  .orient  = {GL_ANGX, GL_ANGY, GL_ANGZ},
  .angstep = DEF_ANGSTEP,
  .fov  = DEF_FOV,
#endif
};

t_type_elem pingelem[PE_MAX] = {
  [PE_NO]   = { .type = PE_NO,   .enable = true, .name = "" },
  [PE_HOST] = { .type = PE_HOST, .enable = true, .name = ELEM_HOST_HDR, .tip = ELEM_HOST_TIP },
  [PE_AS]   = { .type = PE_AS,   .enable = true, .name = ELEM_AS_HDR,   .tip = ELEM_AS_TIP   },
  [PE_CC]   = { .type = PE_CC,   .enable = true, .name = ELEM_CC_HDR,   .tip = ELEM_CC_TIP   },
  [PE_DESC] = { .type = PE_DESC,                 .name = ELEM_DESC_HDR, .tip = ELEM_DESC_TIP },
  [PE_RT]   = { .type = PE_RT,                   .name = ELEM_RT_HDR,   .tip = ELEM_RT_TIP   },
  [PE_FILL] = { .type = PE_FILL, .enable = true, .name = "" },
  [PE_LOSS] = { .type = PE_LOSS, .enable = true, .name = ELEM_LOSS_HDR, .tip = ELEM_LOSS_TIP },
  [PE_SENT] = { .type = PE_SENT, .enable = true, .name = ELEM_SENT_HDR, .tip = ELEM_SENT_TIP },
  [PE_RECV] = { .type = PE_RECV,                 .name = ELEM_RECV_HDR, .tip = ELEM_RECV_TIP },
  [PE_LAST] = { .type = PE_LAST, .enable = true, .name = ELEM_LAST_HDR, .tip = ELEM_LAST_TIP },
  [PE_BEST] = { .type = PE_BEST, .enable = true, .name = ELEM_BEST_HDR, .tip = ELEM_BEST_TIP },
  [PE_WRST] = { .type = PE_WRST, .enable = true, .name = ELEM_WRST_HDR, .tip = ELEM_WRST_TIP },
  [PE_AVRG] = { .type = PE_AVRG, .enable = true, .name = ELEM_AVRG_HDR, .tip = ELEM_AVRG_TIP },
  [PE_JTTR] = { .type = PE_JTTR, .enable = true, .name = ELEM_JTTR_HDR, .tip = ELEM_JTTR_TIP },
};

t_type_elem graphelem[GX_MAX] = {
  [GE_NO]   = { .type = GE_NO,   .enable = true, .name = "" },
  [GE_DASH] = { .type = GE_DASH, .enable = true, .name = GRLG_DASH_HEADER },
  [GE_AVJT] = { .type = GE_AVJT, .enable = true, .name = GRLG_AVJT_HEADER },
  [GE_CCAS] = { .type = GE_CCAS, .enable = true, .name = GRLG_CCAS_HEADER },
  [GE_LGHN] = { .type = GE_LGHN, .enable = true, .name = GRLG_LGHN_HDR    },
  [GX_MEAN] = { .type = GX_MEAN,                 .name = GREX_MEAN_HDR    },
  [GX_JRNG] = { .type = GX_JRNG,                 .name = GREX_JRNG_HDR    },
  [GX_AREA] = { .type = GX_AREA,                 .name = GREX_AREA_HDR    },
};

#ifdef WITH_PLOT
t_type_elem plotelem[D3_MAX] = {
  [D3_BACK] = { .type = D3_BACK, .enable = true, .name = PLEL_BACK_HDR },
  [D3_AXIS] = { .type = D3_AXIS, .enable = true, .name = PLEL_AXIS_HDR },
  [D3_GRID] = { .type = D3_GRID, .enable = true, .name = PLEL_GRID_HDR },
  [D3_ROTR] = { .type = D3_ROTR, .enable = true, .name = PLEL_ROTR_HDR },
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
    {ENT_BOOL_HOST, PE_HOST}, // h
    {ENT_BOOL_AS,   PE_AS},   // a
    {ENT_BOOL_CC,   PE_CC},   // c
    {ENT_BOOL_DESC, PE_DESC}, // d
    {ENT_BOOL_RT,   PE_RT},   // r
  },
  [STAT_CHAR] = {
    {ENT_BOOL_LOSS, PE_LOSS}, // l
    {ENT_BOOL_SENT, PE_SENT}, // s
    {ENT_BOOL_RECV, PE_RECV}, // r
    {ENT_BOOL_LAST, PE_LAST}, // m
    {ENT_BOOL_BEST, PE_BEST}, // b
    {ENT_BOOL_WRST, PE_WRST}, // w
    {ENT_BOOL_AVRG, PE_AVRG}, // a
    {ENT_BOOL_JTTR, PE_JTTR}, // j
  },
  [GRLG_CHAR] = {
    {ENT_BOOL_AVJT, GE_AVJT}, // d
    {ENT_BOOL_CCAS, GE_CCAS}, // c
    {ENT_BOOL_LGHN, GE_LGHN}, // h
  },
  [GREX_CHAR] = {
    {ENT_BOOL_MEAN, GX_MEAN}, // l
    {ENT_BOOL_JRNG, GX_JRNG}, // r
    {ENT_BOOL_AREA, GX_AREA}, // a
  },
#ifdef WITH_PLOT
  [PLEL_CHAR] = {
    {ENT_BOOL_PLBK, D3_BACK}, // b
    {ENT_BOOL_PLAX, D3_AXIS}, // a
    {ENT_BOOL_PLGR, D3_GRID}, // g
    {ENT_BOOL_PLRR, D3_ROTR}, // r
  },
#endif
};

int char2ndx(int cat, gboolean ent, char c) {
  int n = -1, len = G_N_ELEMENTS(char_ndxs);
  if ((cat >= 0) && (cat < len)) switch (cat) {
    case INFO_CHAR:
    case STAT_CHAR:
    case GRLG_CHAR:
    case GREX_CHAR:
#ifdef WITH_PLOT
    case PLEL_CHAR:
#endif
    { char *p = strchr(char_pattern[cat], c); if (p) {
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
    case PE_HOST: n = ENT_BOOL_HOST; break;
    case PE_AS:   n = ENT_BOOL_AS;   break;
    case PE_CC:   n = ENT_BOOL_CC;   break;
    case PE_DESC: n = ENT_BOOL_DESC; break;
    case PE_RT:   n = ENT_BOOL_RT;   break;
    case PE_LOSS: n = ENT_BOOL_LOSS; break;
    case PE_SENT: n = ENT_BOOL_SENT; break;
    case PE_RECV: n = ENT_BOOL_RECV; break;
    case PE_LAST: n = ENT_BOOL_LAST; break;
    case PE_BEST: n = ENT_BOOL_BEST; break;
    case PE_WRST: n = ENT_BOOL_WRST; break;
    case PE_AVRG: n = ENT_BOOL_AVRG; break;
    case PE_JTTR: n = ENT_BOOL_JTTR; break;
  }
  return n;
}

static int graphelem_type2ent(int type) {
  int n = -1;
  switch (type) {
    case GE_AVJT: n = ENT_BOOL_AVJT; break;
    case GE_CCAS: n = ENT_BOOL_CCAS; break;
    case GE_LGHN: n = ENT_BOOL_LGHN; break;
    case GX_MEAN: n = ENT_BOOL_MEAN; break;
    case GX_JRNG: n = ENT_BOOL_JRNG; break;
    case GX_AREA: n = ENT_BOOL_AREA; break;
  }
  return n;
}

#ifdef WITH_PLOT
static int plotelem_type2ent(int type) {
  int n = -1;
  switch (type) {
    case D3_BACK: n = ENT_BOOL_PLBK; break;
    case D3_AXIS: n = ENT_BOOL_PLAX; break;
    case D3_GRID: n = ENT_BOOL_PLGR; break;
    case D3_ROTR: n = ENT_BOOL_PLRR; break;
  }
  return n;
}
#endif

t_elem_desc info_desc = { .elems = pingelem, .mm = { .min = PE_HOST, .max = PE_RT },
  .cat = INFO_CHAR, .patt = INFO_PATT, .t2n = pingelem_type2ndx, .t2e = pingelem_type2ent };
t_elem_desc stat_desc = { .elems = pingelem, .mm = { .min = PE_LOSS, .max = PE_JTTR },
  .cat = STAT_CHAR, .patt = STAT_PATT, .t2n = pingelem_type2ndx, .t2e = pingelem_type2ent };

t_elem_desc grlg_desc = { .elems = graphelem, .mm = { .min = GE_AVJT, .max = GE_LGHN },
  .cat = GRLG_CHAR, .patt = GRLG_PATT, .t2n = graphelem_type2ndx, .t2e = graphelem_type2ent };
t_elem_desc grex_desc = { .elems = graphelem, .mm = { .min = GX_MEAN, .max = GX_AREA },
  .cat = GREX_CHAR, .patt = GREX_PATT, .t2n = graphelem_type2ndx, .t2e = graphelem_type2ent };

#ifdef WITH_PLOT
t_elem_desc plot_desc = { .elems = plotelem, .mm = { .min = D3_BACK, .max = D3_ROTR },
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
  return ((ndx >= 0) && (ndx < G_N_ELEMENTS(graphelem))) ? graphelem[ndx].enable : false;
}

#ifdef WITH_PLOT
gboolean* plotelem_enabler(int type) { return elem_enabler(type, plotelem, G_N_ELEMENTS(plotelem)); }

gboolean is_plelem_enabled(int type) {
  int ndx = plotelem_type2ndx(type);
  return ((ndx >= 0) && (ndx < G_N_ELEMENTS(plotelem))) ? plotelem[ndx].enable : false;
}
#endif

#define CLEAN_ELEM_LOOP(elems, min, max) { for (int i = 0; i < G_N_ELEMENTS(elems); i++) \
  if ((min <= elems[i].type) && (elems[i].type <= max)) elems[i].enable = false; }

void clean_elems(int type) {
  switch (type) {
    case ENT_EXP_INFO: CLEAN_ELEM_LOOP(pingelem,  PE_HOST, PE_RT);   break;
    case ENT_EXP_STAT: CLEAN_ELEM_LOOP(pingelem,  PE_LOSS, PE_JTTR); break;
    case ENT_EXP_LGFL: CLEAN_ELEM_LOOP(graphelem, GE_AVJT, GE_LGHN); break;
    case ENT_EXP_GREX: CLEAN_ELEM_LOOP(graphelem, GX_MEAN, GX_AREA); break;
#ifdef WITH_PLOT
    case ENT_EXP_PLEL: CLEAN_ELEM_LOOP(plotelem,  D3_BACK, D3_ROTR); break;
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
  if (!list) return NULL;
  t_ref *ref = ref_new(hop, ndx);
  if (!ref) return NULL;
  { GSList *elem = g_slist_find_custom(*list, ref, (GCompareFunc)ref_cmp);
    if (elem) { g_free(ref); return elem; } }
  *list = g_slist_prepend(*list, ref);
  if (g_slist_length(*list) > REF_MAX)
    *list = g_slist_delete_link(*list, g_slist_last(*list));
  return *list;
}

