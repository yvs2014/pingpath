
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "option.h"
#include "parser.h"
#include "pinger.h"
#include "stat.h"
#include "style.h"
#include "notifier.h"
#include "tabs/aux.h"
#include "tabs/ping.h"
#include "tabs/graph.h"
#ifdef WITH_PLOT
#include "tabs/plot.h"

enum { MIN_VIEW_ANGLE = -180, MAX_VIEW_ANGLE = 180 };
#endif

t_opts opts = { .target = NULL, .range = {.min = MINTTL - 1, .max = MAXTTL},
  .dns = DEF_DNS, .whois = DEF_WHOIS, .cycles = DEF_CYCLES, .qos = DEF_QOS, .size = DEF_PSIZE,
  .timeout = DEF_TOUT, .tout_usec = DEF_TOUT * G_USEC_PER_SEC, .logmax = DEF_LOGMAX,
  .graph = GRAPH_TYPE_CURVE, .legend = DEF_LEGEND,
  .darktheme = DEF_DARK_MAIN, .darkgraph = DEF_DARK_GRAPH,
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

typedef struct ent_rad_map {
  unsigned ndx;
  int val;
  char *str;
} t_ent_rad_map;

typedef struct ent_rad {
  t_ent_exp_common c;
  int *pval;
  GCallback cb;
  t_ent_rad_map map[SUBLIST_MAX];
} t_ent_rad;

#ifdef WITH_PLOT
enum { ENT_SPN_ROT0, ENT_SPN_ROT1, ENT_SPN_ROT2, ENT_SPN_ANGS, ENT_SPN_ROT_MAX };
#endif

t_ent_bool ent_bool[] = {
  [ENT_BOOL_DNS]  = { .en.type = ENT_BOOL_DNS,  .pval = &opts.dns },
  [ENT_BOOL_HOST] = { .en.type = ENT_BOOL_HOST, .valfn = pingelem_enabler, .valtype = PE_HOST },
  [ENT_BOOL_AS]   = { .en.type = ENT_BOOL_AS,   .valfn = pingelem_enabler, .valtype = PE_AS   },
  [ENT_BOOL_CC]   = { .en.type = ENT_BOOL_CC,   .valfn = pingelem_enabler, .valtype = PE_CC   },
  [ENT_BOOL_DESC] = { .en.type = ENT_BOOL_DESC, .valfn = pingelem_enabler, .valtype = PE_DESC },
  [ENT_BOOL_RT]   = { .en.type = ENT_BOOL_RT,   .valfn = pingelem_enabler, .valtype = PE_RT   },
  [ENT_BOOL_LOSS] = { .en.type = ENT_BOOL_LOSS, .valfn = pingelem_enabler, .valtype = PE_LOSS },
  [ENT_BOOL_SENT] = { .en.type = ENT_BOOL_SENT, .valfn = pingelem_enabler, .valtype = PE_SENT },
  [ENT_BOOL_RECV] = { .en.type = ENT_BOOL_RECV, .valfn = pingelem_enabler, .valtype = PE_RECV },
  [ENT_BOOL_LAST] = { .en.type = ENT_BOOL_LAST, .valfn = pingelem_enabler, .valtype = PE_LAST },
  [ENT_BOOL_BEST] = { .en.type = ENT_BOOL_BEST, .valfn = pingelem_enabler, .valtype = PE_BEST },
  [ENT_BOOL_WRST] = { .en.type = ENT_BOOL_WRST, .valfn = pingelem_enabler, .valtype = PE_WRST },
  [ENT_BOOL_AVRG] = { .en.type = ENT_BOOL_AVRG, .valfn = pingelem_enabler, .valtype = PE_AVRG },
  [ENT_BOOL_JTTR] = { .en.type = ENT_BOOL_JTTR, .valfn = pingelem_enabler, .valtype = PE_JTTR },
  [ENT_BOOL_MN_DARK] = { .en.type = ENT_BOOL_MN_DARK, .pval = &opts.darktheme },
  [ENT_BOOL_GR_DARK] = { .en.type = ENT_BOOL_GR_DARK, .pval = &opts.darkgraph },
#ifdef WITH_PLOT
  [ENT_BOOL_PL_DARK] = { .en.type = ENT_BOOL_PL_DARK, .pval = &opts.darkplot },
#endif
  [ENT_BOOL_LGND] = { .en.type = ENT_BOOL_LGND, .pval = &opts.legend },
  [ENT_BOOL_AVJT] = { .en.type = ENT_BOOL_AVJT, .valfn = graphelem_enabler, .valtype = GE_AVJT },
  [ENT_BOOL_CCAS] = { .en.type = ENT_BOOL_CCAS, .valfn = graphelem_enabler, .valtype = GE_CCAS },
  [ENT_BOOL_LGHN] = { .en.type = ENT_BOOL_LGHN, .valfn = graphelem_enabler, .valtype = GE_LGHN },
  [ENT_BOOL_MEAN] = { .en.type = ENT_BOOL_MEAN, .valfn = graphelem_enabler, .valtype = GX_MEAN },
  [ENT_BOOL_JRNG] = { .en.type = ENT_BOOL_JRNG, .valfn = graphelem_enabler, .valtype = GX_JRNG },
  [ENT_BOOL_AREA] = { .en.type = ENT_BOOL_AREA, .valfn = graphelem_enabler, .valtype = GX_AREA },
#ifdef WITH_PLOT
  [ENT_BOOL_PLBK] = { .en.type = ENT_BOOL_PLBK, .valfn = plotelem_enabler, .valtype = D3_BACK },
  [ENT_BOOL_PLAX] = { .en.type = ENT_BOOL_PLAX, .valfn = plotelem_enabler, .valtype = D3_AXIS },
  [ENT_BOOL_PLGR] = { .en.type = ENT_BOOL_PLGR, .valfn = plotelem_enabler, .valtype = D3_GRID },
  [ENT_BOOL_PLRR] = { .en.type = ENT_BOOL_PLRR, .valfn = plotelem_enabler, .valtype = D3_ROTR },
  [ENT_BOOL_GLOB] = { .en.type = ENT_BOOL_GLOB, .pval = &opts.rglob },
#endif
};

t_ent_str ent_str[] = {
  [ENT_STR_CYCLES] = { .len = 6,  .width = 6,
    .range = { .min = CYCLES_MIN, .max = CYCLES_MAX },
    .en.type = ENT_STR_CYCLES, .pint = &opts.cycles,  .idef = DEF_CYCLES },
  [ENT_STR_IVAL]   = { .len = 4,  .width = 6,
    .range = { .min = IVAL_MIN,   .max = IVAL_MAX },
    .en.type = ENT_STR_IVAL,   .pint = &opts.timeout, .idef = DEF_TOUT },
  [ENT_STR_QOS]    = { .len = 3,  .width = 6, .range = { .max = QOS_MAX },
    .en.type = ENT_STR_QOS,    .pint = &opts.qos,     .idef = DEF_QOS },
  [ENT_STR_PLOAD]  = { .len = 48, .width = 6,
    .en.type = ENT_STR_PLOAD,  .pstr = opts.pad,      .sdef = DEF_PPAD,
    .slen = sizeof(opts.pad) },
  [ENT_STR_PSIZE]  = { .len = 4,  .width = 6,
    .range = { .min = PSIZE_MIN,  .max = PSIZE_MAX },
    .en.type = ENT_STR_PSIZE,  .pint = &opts.size,    .idef = DEF_PSIZE },
  [ENT_STR_LOGMAX] = { .len = 3,  .width = 6, .range = { .max = LOGMAX_MAX },
    .en.type = ENT_STR_LOGMAX, .pint = &opts.logmax,  .idef = DEF_LOGMAX },
};

static t_ent_exp ent_exp[] = {
  [ENT_EXP_INFO] = { .c.en.type = ENT_EXP_INFO },
  [ENT_EXP_STAT] = { .c.en.type = ENT_EXP_STAT },
  [ENT_EXP_LGFL] = { .c.en.type = ENT_EXP_LGFL },
  [ENT_EXP_GREX] = { .c.en.type = ENT_EXP_GREX },
#ifdef WITH_PLOT
  [ENT_EXP_PLEL] = { .c.en.type = ENT_EXP_PLEL },
#endif
};

static void minttl_cb(GtkSpinButton*, t_ent_spn_elem*);
static void maxttl_cb(GtkSpinButton*, t_ent_spn_elem*);
t_minmax opt_mm_ttl = {MINTTL, MAXTTL};

#ifdef WITH_PLOT
#define MIN_COL_VAL 0
#define MAX_COL_VAL 0xff
static void colgrad_cb(GtkSpinButton*, t_ent_spn_aux*);
t_minmax opt_mm_col = {MIN_COL_VAL, MAX_COL_VAL};
//
static void rotation_cb(GtkSpinButton*, t_ent_spn_aux*);
t_minmax opt_mm_rot = {MIN_VIEW_ANGLE, MAX_VIEW_ANGLE};
t_minmax opt_mm_ang = {1, MAX_VIEW_ANGLE};
//
static void scalefov_cb(GtkSpinButton*, t_ent_spn_aux*);
t_minmax opt_mm_fov = {DEF_FOV * 2 / 3, DEF_FOV * 2};
#endif

t_ent_spn ent_spn[] = {
  [ENT_SPN_TTL] = { .c.en.type = ENT_SPN_TTL, .list = {
    { .bond = true, .kind = MINIMAX_SPIN, .aux = {
      { .cb = G_CALLBACK(minttl_cb), .pval = &opts.range.min, .def = MINTTL,
        .mm = &opt_mm_ttl, .mmval = &opts.range },
      { .cb = G_CALLBACK(maxttl_cb), .pval = &opts.range.max, .def = MAXTTL,
        .mm = &opt_mm_ttl, .mmval = &opts.range }
  }}}},
#ifdef WITH_PLOT
  [ENT_SPN_COLOR] = { .c = { .en.type = ENT_SPN_COLOR, .atrun = true },
    .list = {
      { .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.rcol.min, .def = DEF_RCOL_FROM,
          .mm = &opt_mm_col, .mmval = &opts.rcol },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.rcol.max, .def = DEF_RCOL_TO,
          .mm = &opt_mm_col, .mmval = &opts.rcol }}},
      { .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.gcol.min, .def = DEF_GCOL_FROM,
          .mm = &opt_mm_col, .mmval = &opts.gcol },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.gcol.max, .def = DEF_GCOL_TO,
          .mm = &opt_mm_col, .mmval = &opts.gcol }}},
      { .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.bcol.min, .def = DEF_BCOL_FROM,
          .mm = &opt_mm_col, .mmval = &opts.bcol },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.bcol.max, .def = DEF_BCOL_TO,
          .mm = &opt_mm_col, .mmval = &opts.bcol }}},
  }},
  [ENT_SPN_GLOBAL] = { .c = { .en.type = ENT_SPN_GLOBAL, .atrun = true },
    .list = {
      { .kind = ROTOR_COLUMN, .aux = {
        [ENT_SPN_ROT0] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 0, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep },
        [ENT_SPN_ROT1] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 1, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep },
        [ENT_SPN_ROT2] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 2, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep },
        [ENT_SPN_ANGS] = { .cb = G_CALLBACK(rotation_cb), .pval = &opts.angstep,
          .def = DEF_ANGSTEP, .sn = ENT_SPN_GLOBAL, .mm = &opt_mm_ang },
  }}}},
  [ENT_SPN_LOCAL] = { .c = { .en.type = ENT_SPN_LOCAL, .atrun = true },
    .list = {
      { .kind = ROTOR_COLUMN, .aux = {
        [ENT_SPN_ROT0] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 1, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep, .rev = true },
        [ENT_SPN_ROT1] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 2, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep },
        [ENT_SPN_ROT2] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient,
          .pn = 0, .wrap = 360, .mm = &opt_mm_rot, .step = &opts.angstep, .rev = true },
        [ENT_SPN_ANGS] = { .cb = G_CALLBACK(rotation_cb), .pval = &opts.angstep,
          .def = DEF_ANGSTEP, .sn = ENT_SPN_LOCAL, .mm = &opt_mm_ang },
  }}}},
  [ENT_SPN_FOV] = { .c = { .en.type = ENT_SPN_FOV, .atrun = true },
    .list = {
      { .kind = SCALE_SPIN, .aux = {
        { .cb = G_CALLBACK(scalefov_cb), .pval = &opts.fov,
          .def = DEF_FOV, .mm = &opt_mm_fov }}},
  }},
#endif
};

static void graph_type_cb(void);

static t_ent_rad ent_rad[] = {
  [ENT_RAD_IPV] = { .c.en.type = ENT_RAD_IPV,
    .pval = &opts.ipv,
    .map = {
      { .ndx = ENT_RAD_IPV },
      { .ndx = ENT_RAD_IPV, .val = 4 },
      { .ndx = ENT_RAD_IPV, .val = 6 },
    }},
  [ENT_RAD_GRAPH] = { .c = {.en.type = ENT_RAD_GRAPH, .atrun = true },
    .pval = &opts.graph, .cb = graph_type_cb,
    .map = {
      { .ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_DOT   },
      { .ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_LINE  },
      { .ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_CURVE },
    }},
};

static gboolean opt_silent;
#define OPT_NOTIFY(...) do { if (!opt_silent) notifier_inform(__VA_ARGS__); } \
  while (0)

//

static gboolean check_bool_val(GtkCheckButton *check, t_ent_bool *en, void (*update)(void)) {
  gboolean state = gtk_check_button_get_active(check), okay = false;
  if (en) {
    gboolean *pbool = EN_BOOLPTR(en);
    if (pbool && (*pbool != state)) {
      *pbool = state; okay = true;
      if (update) update();
    }
    const char *name = en->en.name;
    if (name && (*name == '_'))
      name++;
    if (en->prefix) OPT_NOTIFY("%s: %s %s", en->prefix, name, onoff(state));
    else            OPT_NOTIFY("%s: %s",                name, onoff(state));
  }
  return okay;
}

static void set_ed_texthint(t_ent_str *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(en->input));
  int  *pint = en->pint;
  char *pstr = en->pstr;
  gtk_editable_delete_text(GTK_EDITABLE(en->input), 0, -1);
  if (pint && (*pint != en->idef)) {
    g_snprintf(en->buff, sizeof(en->buff), "%d", *pint);
    gtk_editable_set_text(GTK_EDITABLE(en->input), en->buff);
  } else if (pstr && strncmp(pstr, en->sdef, en->slen)) {
    g_strlcpy(en->buff, pstr, sizeof(en->buff));
    gtk_editable_set_text(GTK_EDITABLE(en->input), en->buff);
  } else if (en->hint[0])
    gtk_entry_set_placeholder_text(GTK_ENTRY(en->input), en->hint);
}

static void toggled_dns(void) { stat_reset_cache(); notifier_legend_update(); pinger_update_tabs(NULL); }

static void toggle_colors(void) {
  tab_dependent(NULL); style_set();
  tab_reload_theme();  drawtab_refresh();
  notifier_legend_refresh();
}

static void toggle_legend(void) { if (opts.graph) notifier_set_visible(NT_LEGEND_NDX, opts.legend); graphtab_refresh(); }
#ifdef WITH_PLOT
static void toggle_pl_elems(void) { plottab_refresh(PL_PARAM_AT); }
static void toggle_pl_rotor(void) { notifier_set_visible(NT_ROTOR_NDX, is_plelem_enabled(D3_ROTR)); toggle_pl_elems(); }
#endif

static void toggle_cb(GtkCheckButton *check, t_ent_bool *en) {
  if (!GTK_IS_CHECK_BUTTON(check) || !en) return;
  switch (en->en.type) {
    case ENT_BOOL_DNS:
      check_bool_val(check, en, toggled_dns);
      break;
    case ENT_BOOL_HOST:
    case ENT_BOOL_LOSS:
    case ENT_BOOL_SENT:
    case ENT_BOOL_RECV:
    case ENT_BOOL_LAST:
    case ENT_BOOL_BEST:
    case ENT_BOOL_WRST:
    case ENT_BOOL_AVRG:
    case ENT_BOOL_JTTR:
      check_bool_val(check, en, pingtab_vis_cols);
      break;
    case ENT_BOOL_AS:
    case ENT_BOOL_CC:
    case ENT_BOOL_DESC:
    case ENT_BOOL_RT:
      if (check_bool_val(check, en, pingtab_vis_cols)) notifier_legend_vis_rows(-1);
      stat_whois_enabler();
      if (opts.whois) stat_run_whois_resolv();
      break;
    case ENT_BOOL_MN_DARK:
    case ENT_BOOL_GR_DARK:
#ifdef WITH_PLOT
    case ENT_BOOL_PL_DARK:
#endif
      check_bool_val(check, en, toggle_colors);
      break;
    case ENT_BOOL_LGND:
      check_bool_val(check, en, toggle_legend);
      break;
    case ENT_BOOL_AVJT:
    case ENT_BOOL_CCAS:
    case ENT_BOOL_LGHN:
      if (check_bool_val(check, en, NULL)) { notifier_legend_vis_rows(-1); graphtab_refresh(); }
      break;
    case ENT_BOOL_MEAN:
    case ENT_BOOL_JRNG:
    case ENT_BOOL_AREA:
      if (check_bool_val(check, en, NULL)) { if (opts.legend) notifier_legend_update(); graphtab_refresh(); }
      break;
#ifdef WITH_PLOT
    case ENT_BOOL_PLBK:
    case ENT_BOOL_PLAX:
    case ENT_BOOL_PLGR:
      check_bool_val(check, en, toggle_pl_elems);
      break;
    case ENT_BOOL_PLRR:
      check_bool_val(check, en, toggle_pl_rotor);
      break;
    case ENT_BOOL_GLOB:
      check_bool_val(check, en, option_up_menu_ext);
      break;
#endif
    default: break;
  }
}

static void radio_cb(GtkCheckButton *check, t_ent_rad_map *map) {
  if (!GTK_IS_CHECK_BUTTON(check) || !map) return;
  unsigned ndx = map->ndx; if (ndx >= G_N_ELEMENTS(ent_rad)) return;
  t_ent_rad *en = &ent_rad[ndx];
  int *pval = en->pval, type = en->c.en.type;
  if (pval && ((type == ENT_RAD_IPV) || (type == ENT_RAD_GRAPH))) {
    gboolean selected = gtk_check_button_get_active(check);
    if (selected && (map->val != *pval)) {
      *(en->pval) = map->val;
      if (en->cb) en->cb();
      if (map->str) OPT_NOTIFY("%s: %s", en->c.en.name, map->str);
      else          OPT_NOTIFY("%s: %d", en->c.en.name, map->val);
    }
  }
}

static void arrow_cb(GtkToggleButton *self G_GNUC_UNUSED, t_ent_exp_common *en) {
  if (en && GTK_IS_TOGGLE_BUTTON(en->arrow)) {
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(en->arrow));
    g_object_set(G_OBJECT(en->arrow), ICON_PROP, active ? GO_UP_ICON : GO_DOWN_ICON, NULL);
    if (GTK_IS_WIDGET(en->sub)) gtk_widget_set_visible(en->sub, active);
    DEBUG("%s: %d", en->en.name, active);
  }
}

static void input_cb(GtkWidget *input, t_ent_str *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(input));
  const char *got = gtk_editable_get_text(GTK_EDITABLE(input));
  switch (en->en.type) {
    case ENT_STR_CYCLES:
    case ENT_STR_IVAL:
    case ENT_STR_LOGMAX:
    case ENT_STR_QOS:
    case ENT_STR_PSIZE: {
      gboolean okay = parser_mmint(got, en->en.name, en->range, en->pint);
      if (okay) {
        if (en->pint) {
          int n = *en->pint;
          /*extra*/ if (en->en.type == ENT_STR_IVAL) opts.tout_usec = n * G_USEC_PER_SEC;
          OPT_NOTIFY("%s: %d", en->en.name, n);
        }
      } else set_ed_texthint(en);
      if (en->en.type == ENT_STR_IVAL) drawtab_refresh(); // rescale time-axis
    } break;
    case ENT_STR_PLOAD: {
      char *pad = parser_str(got, en->en.name, OPT_TYPE_PAD);
      if (pad) {
        if (en->pstr) g_strlcpy(en->pstr, pad, en->slen);
        OPT_NOTIFY("%s: %s", en->en.name, pad);
        g_free(pad);
      } else set_ed_texthint(en);
    } break;
    default: break;
  }
  DEBUG("%s: %s", en->en.name, got);
}

static void minttl_cb(GtkSpinButton *spin, t_ent_spn_elem *en) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !en) return;
  int *pmin = en->aux[0].pval; if (!pmin) return;
  int got = gtk_spin_button_get_value_as_int(spin);
  int val = got - 1;
  if (*pmin == val) return;
  t_ent_spn_aux *aux = &en->aux[1];
  int *plim = aux->pval;
  const t_minmax *minmax = aux->mm ? aux->mm : &opt_mm_ttl;
  int max = plim ? *plim : minmax->max;
  if (pinger_within_range(minmax->min, max, got)) {
    OPT_NOTIFY("%s: %d <=> %d", en->title, got, max);
    *pmin = val;
    // then adjust right:range
    double cp_min = NAN, cp_max = NAN;
    gtk_spin_button_get_range(aux->spin, &cp_min, &cp_max);
    if (got != cp_min) {
      int cp_val = gtk_spin_button_get_value_as_int(aux->spin);
      if (cp_val >= got) // avoid callback triggering
        gtk_spin_button_set_range(aux->spin, got, cp_max);
    }
  } else WARN("out-of-range[%d,%d]: %d", minmax->min, max, got);
}

static void maxttl_cb(GtkSpinButton *spin, t_ent_spn_elem *en) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !en) return;
  int *plim = en->aux[1].pval; if (!plim) return;
  int got = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
  if (*plim == got) return;
  t_ent_spn_aux *aux = &en->aux[0];
  int *pmin = aux->pval;
  int min = (pmin ? *pmin : 0) + 1;
  const t_minmax *minmax = aux->mm ? aux->mm : &opt_mm_ttl;
  if (pinger_within_range(min, minmax->max, got)) {
    OPT_NOTIFY("%s: %d <=> %d", en->title, min, got);
    *plim = got;
    // then adjust left:range
    double cp_min = NAN, cp_max = NAN;
    gtk_spin_button_get_range(aux->spin, &cp_min, &cp_max);
    if (got != cp_max) {
      int cp_val = gtk_spin_button_get_value_as_int(aux->spin);
      if (cp_val <= got) // avoid callback triggering
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(aux->spin), cp_min, got);
    }
  } else WARN("out-of-range[%d,%d]: %d", min, minmax->max, got);
}

#ifdef WITH_PLOT
static void colgrad_cb(GtkSpinButton *spin, t_ent_spn_aux *aux) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !aux) return;
  int *pval = aux->pval; if (!pval) return;
  int got = gtk_spin_button_get_value_as_int(spin);
  if (*pval == got) return;
  const t_minmax *minmax = aux->mm ? aux->mm : &opt_mm_col;
  if ((minmax->min <= got) && (got <= minmax->max)) {
    *pval = got;
    if (aux->mmval)
      OPT_NOTIFY("%s: %d(0x%02x) <=> %d(0x%02x)", aux->prfx,
        aux->mmval->min, aux->mmval->min, aux->mmval->max, aux->mmval->max);
    else
      OPT_NOTIFY("%s: %d (0x%02x)", aux->prfx, got, got);
    plottab_refresh(PL_PARAM_COLOR);
  } else WARN("out-of-range[%d,%d]: %d", minmax->min, minmax->max, got);
}

void set_rotor_n_redraw(int step, gboolean rev, int n) {
  if (!step) return;
  int q[4] = { 0, 0, 0, rev ? -step : step }; q[n] = 1;
  for (int i = 0; i < 3; i++) q[i] = (i == n);
  plottab_on_trans_opts(q); plottab_redraw();
}

static void rotation_cb(GtkSpinButton *spin, t_ent_spn_aux *aux) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !aux) return;
  int *pval = &aux->pval[aux->pn];
  int want = gtk_spin_button_get_value_as_int(spin);
  int step = want - *pval; if (aux->wrap) step %= aux->wrap;
  if (!step) return;
  const t_minmax *minmax = aux->mm ? aux->mm : &opt_mm_rot;
  if ((minmax->min <= want) && (want <= minmax->max)) {
    OPT_NOTIFY("%s: %d", aux->prfx, want);
    if (aux->sn) {
      *pval = want; t_ent_spn_elem *list = ent_spn[aux->sn].list;
      if (list) for (int i = 0; i < ENT_SPN_ANGS; i++) { // adjust angle entries' step
        GtkSpinButton *spin = list->aux[i].spin;
        if (GTK_IS_SPIN_BUTTON(spin)) gtk_spin_button_set_increments(spin, want, want * 6); }
    } else { *pval = want; set_rotor_n_redraw(step, aux->rev, aux->pn); }
  }
}

static void scalefov_cb(GtkSpinButton *spin, t_ent_spn_aux *aux) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !aux) return;
  int *pval = aux->pval; if (!pval) return;
  int got = gtk_spin_button_get_value_as_int(spin);
  const t_minmax *minmax = aux->mm ? aux->mm : &opt_mm_fov;
  if ((*pval == got) || (got < minmax->min) || (got > minmax->max)) return;
  *pval = got;
  OPT_NOTIFY("%s: %d°", aux->prfx, got);
  plottab_refresh(PL_PARAM_FOV);
}
#endif

static void graph_type_cb(void) {
  if (!pinger_state.pause) graphtab_update();
  graphtab_refresh();
}

static GtkWidget* label_box(const char *name, const char *unit) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN * 2);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  char *name_u = unit ?
    g_strdup_printf("%s, %s", name, unit):
    g_strdup(name);
  GtkWidget *label = gtk_label_new(name_u);
  g_free(name_u);
  g_return_val_if_fail(GTK_IS_LABEL(label), box);
  gtk_label_set_use_underline(GTK_LABEL(label), true);
  gtk_box_append(GTK_BOX(box), label);
  if (style_loaded) gtk_widget_add_css_class(label, CSS_PAD);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(label, true);
  return box;
}

static GtkWidget* add_labelbox(GtkWidget* list, const char *name, const char *unit) {
  GtkWidget *box = label_box(name, unit);
  if (box) {
    if (GTK_IS_LIST_BOX(list)) gtk_list_box_append(GTK_LIST_BOX(list), box);
    else if (GTK_IS_BOX(list)) gtk_box_append(GTK_BOX(list), box);
  }
  return box;
}

static GtkWidget* add_opt_check(GtkWidget* list, t_ent_bool *en) {
  if (!en) return NULL;
  GtkWidget *box = add_labelbox(list, en->en.name, en->en.unit);
  if (box) {
    GtkWidget *check = gtk_check_button_new();
    g_return_val_if_fail(GTK_IS_CHECK_BUTTON(check), box);
    gtk_box_append(GTK_BOX(box), check);
    if (style_loaded) gtk_widget_add_css_class(check, CSS_RPAD);
    gtk_widget_set_halign(check, GTK_ALIGN_END);
    g_signal_connect(check, EV_TOGGLE, G_CALLBACK(toggle_cb), en);
    en->check = GTK_CHECK_BUTTON(check);
    gboolean *pbool = EN_BOOLPTR(en);
    if (pbool) gtk_check_button_set_active(en->check, *pbool);
  }
  return box;
}

static GtkWidget* add_opt_enter(GtkWidget* list, t_ent_str *en) {
  if (!en) return NULL;
  GtkWidget *box = add_labelbox(list, en->en.name, en->en.unit);
  if (box) {
    en->box = box;
    en->input = gtk_entry_new();
    g_return_val_if_fail(GTK_IS_ENTRY(en->input), box);
    gtk_box_append(GTK_BOX(box), en->input);
    if (style_loaded) gtk_widget_add_css_class(en->input, CSS_NOPAD);
    gtk_widget_set_halign(en->input, GTK_ALIGN_END);
    gtk_entry_set_alignment(GTK_ENTRY(en->input), 0.99);
    gtk_entry_set_max_length(GTK_ENTRY(en->input), en->len);
    set_ed_texthint(en);
    gtk_editable_set_editable(GTK_EDITABLE(en->input), true);
    gtk_editable_set_max_width_chars(GTK_EDITABLE(en->input), en->width);
    g_signal_connect(en->input, EV_ACTIVE, G_CALLBACK(input_cb), en);
  }
  return box;
}

static GtkWidget* add_expand_common(GtkWidget* list, t_ent_exp_common *en) {
  if (!en) return NULL;
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  en->box = box;
  gtk_list_box_append(GTK_LIST_BOX(list), box);
  GtkWidget *title = label_box(en->en.name, en->en.unit);
  if (GTK_IS_BOX(title)) {
    gtk_box_append(GTK_BOX(box), title);
    // arrow into title
    en->arrow = gtk_toggle_button_new();
    g_return_val_if_fail(GTK_IS_TOGGLE_BUTTON(en->arrow), box);
    gtk_box_append(GTK_BOX(title), en->arrow);
    if (style_loaded) gtk_widget_add_css_class(en->arrow, CSS_NOFRAME);
    gtk_widget_set_halign(en->arrow, GTK_ALIGN_END);
    g_object_set(G_OBJECT(en->arrow), ICON_PROP, GO_DOWN_ICON, NULL);
    g_signal_connect(en->arrow, EV_TOGGLE, G_CALLBACK(arrow_cb), en);
  }
  en->sub = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(en->sub), box);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(en->sub), GTK_SELECTION_NONE);
  gtk_widget_set_visible(en->sub, false);
  gtk_box_append(GTK_BOX(box), en->sub);
  return box;
}

static GtkWidget* add_opt_expand(GtkWidget* list, t_ent_exp *en) {
  if (!en) return NULL;
  GtkWidget *box = add_expand_common(list, &en->c);
  if (GTK_IS_BOX(box) && en->c.sub && en->desc) {
    t_elem_desc *desc = en->desc; type2ent_fn type2ent = desc->t2e;
    if (type2ent) for (int i = desc->mm.min; i <= desc->mm.max; i++) {
      int type = desc->elems[i].type;
      int ndx = type2ent(type);
      if (ndx < 0) WARN("Unknown %d type", type);
      else add_opt_check(en->c.sub, &ent_bool[ndx]);
    }
  }
  return box;
}

#define SPN_MM_OR_DEF(aux, inc) ((aux).pval ? (*((aux).pval) + (inc)) : (aux).def)
#define SPN_MM_NDX(mm_ndx, inc) ((ndx == (mm_ndx)) ? (en->aux[mm_ndx].def) : SPN_MM_OR_DEF(en->aux[mm_ndx], inc))

static gboolean add_minmax(GtkWidget *box, t_ent_spn_elem *en, int ndx, const int *step, gboolean wrap) {
  gboolean okay = false;
  if (GTK_IS_BOX(box) && en) {
    const t_minmax *mm0 = en->aux[en->bond ? 0 : ndx].mm;
    const t_minmax *mm1 = en->aux[en->bond ? 1 : ndx].mm;
    if (mm0 && mm1) {
      int min = en->bond ? SPN_MM_NDX(0, 1) : mm0->min;
      int max = en->bond ? SPN_MM_NDX(1, 0) : mm1->max;
      GtkWidget *spin = gtk_spin_button_new_with_range(min, max, step ? *step : 1);
      if (GTK_IS_SPIN_BUTTON(spin)) {
        gtk_box_append(GTK_BOX(box), spin);
        int pn = en->aux[ndx].pn;
        int *pval = &en->aux[ndx].pval[pn];
        if (pval) {
          int val = *pval;
          if (en->bond && !ndx) val++;
          gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), val);
        }
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin), wrap);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), true);
        en->aux[ndx].spin = GTK_SPIN_BUTTON(spin);
        if (en->aux[ndx].cb)
          g_signal_connect(spin, EV_VAL_CHANGE, en->aux[ndx].cb, en->bond ? (void*)en : &en->aux[ndx]);
        okay = true;
     } else WARN("gtk_spin_button_new_with_range()");
  }}
  return okay;
}

static void grey_into_box(GtkWidget *box, GtkWidget *widget, gboolean sens, gboolean hexp) {
  if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(widget)) return;
  gtk_widget_set_sensitive(widget, sens);
  gtk_widget_set_hexpand(widget, hexp);
  gtk_box_append(GTK_BOX(box), widget);
}

static GtkWidget* add_opt_range(GtkWidget* listbox, t_ent_spn *en) {
  if (!GTK_LIST_BOX(listbox) || !en) return NULL;
  GtkWidget *box = add_expand_common(listbox, &en->c);
  if (GTK_IS_BOX(box) && en->c.sub) {
    t_ent_spn_elem *elem = en->list;
    for (unsigned n = 0; (n < G_N_ELEMENTS(en->list)) && elem && elem->title; n++, elem++) {
      GtkWidget *subbox = NULL;
      switch (elem->kind) {
#ifdef WITH_PLOT
        case ROTOR_COLUMN: subbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);   break;
#endif
        default: subbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN); break;
      }
      if (!GTK_IS_BOX(subbox)) continue;
      gtk_list_box_append(GTK_LIST_BOX(en->c.sub), subbox);
      switch (elem->kind) {
        case MINIMAX_SPIN:
          add_minmax(subbox, elem, 0, NULL, false);
          grey_into_box(subbox, gtk_image_new_from_icon_name(GO_LEFT_ICON), false, false);
          grey_into_box(subbox, gtk_label_new(elem->delim ? elem->delim : "min max"), false, true);
          grey_into_box(subbox, gtk_image_new_from_icon_name(GO_RIGHT_ICON), false, false);
          add_minmax(subbox, elem, 1, NULL, false);
          break;
#ifdef WITH_PLOT
        case ROTOR_COLUMN:
          add_opt_check(subbox, &ent_bool[ENT_BOOL_GLOB]);
          for (unsigned i = 0; i < ENT_SPN_ROT_MAX; i++) if (elem->aux[i].prfx) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
            if (!row) continue;
            grey_into_box(row, gtk_label_new(elem->aux[i].prfx), true, true);
            add_minmax(row, elem, i, elem->aux[i].step, elem->aux[i].wrap);
            gtk_box_append(GTK_BOX(subbox), row);
            if (/*(i >= ENT_SPN_ROT0) &&*/ i <= ENT_SPN_ROT2) // leave only buttons for rotation spins
              for (GtkWidget *txt = gtk_widget_get_first_child(GTK_WIDGET(elem->aux[i].spin));
                GTK_IS_WIDGET(txt); txt = gtk_widget_get_next_sibling(txt))
                  if (GTK_IS_TEXT(txt)) gtk_widget_set_visible(txt, false);
          }
          break;
        case SCALE_SPIN:
          grey_into_box(subbox, gtk_label_new(elem->aux->prfx), true, true);
          add_minmax(subbox, elem, 0, NULL, false);
          break;
#endif
        default: break;
      }
    }
  }
  return box;
}

static GtkWidget* add_opt_radio(GtkWidget* list, t_ent_rad *en) {
  if (!en) return NULL;
  GtkWidget *box = add_expand_common(list, &en->c);
  if (GTK_IS_BOX(box) && en->c.sub) {
    GtkWidget *group = NULL;
    t_ent_rad_map *map = en->map;
    for (unsigned i = 0; (i < G_N_ELEMENTS(en->map)) && map && map->str; i++, map++) {
      GtkWidget *button = gtk_check_button_new_with_label(map->str);
      g_return_val_if_fail(GTK_CHECK_BUTTON(button), box);
      if (style_loaded) gtk_widget_add_css_class(button, CSS_PAD);
      if (group) gtk_check_button_set_group(GTK_CHECK_BUTTON(button), GTK_CHECK_BUTTON(group));
      else group = button;
      if (en->pval) gtk_check_button_set_active(GTK_CHECK_BUTTON(button), *(en->pval) == map->val);
      g_signal_connect(button, EV_TOGGLE, G_CALLBACK(radio_cb), map);
      gtk_list_box_append(GTK_LIST_BOX(en->c.sub), button);
    }
  }
  return box;
}

static void gtk_compat_list_box_remove_all(GtkListBox *box) {
  g_return_if_fail(GTK_IS_LIST_BOX(box));
#if GTK_CHECK_VERSION(4, 12, 0)
  gtk_list_box_remove_all(box);
#else
  for (GtkWidget *c; (c = gtk_widget_get_first_child(GTK_WIDGET(box)));) gtk_list_box_remove(box, c);
#endif
}

static gboolean create_optmenu(GtkWidget *bar, const char **icons, const char *tooltip, gboolean (*fn)(GtkWidget*)) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  GtkWidget *menu = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(menu), false);
  gboolean okay = true;
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), menu);
  const char *ico = is_sysicon(icons);
  if (!ico) WARN("No icon found for %s", "option menu");
  else gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu), ico);
  if (tooltip) gtk_widget_set_tooltip_text(menu, tooltip);
  GtkWidget *popover = gtk_popover_new();
  if (GTK_IS_POPOVER(popover)) {
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu), popover);
    gtk_popover_present(GTK_POPOVER(popover));
    GtkWidget* list = gtk_list_box_new();
    if (GTK_IS_LIST_BOX(list)) {
      gtk_popover_set_child(GTK_POPOVER(popover), list);
      gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
      gtk_widget_set_halign(list, GTK_ALIGN_FILL);
      gtk_widget_set_hexpand(list, false);
      if (fn) okay = fn(list);
    } else okay = false;
  } else okay = false;
  if (!okay) gtk_header_bar_remove(GTK_HEADER_BAR(bar), menu);
  return okay;
}

#define EN_PR_FMT(ndx, fmt, arg) { g_snprintf(ent_str[ndx].hint, sizeof(ent_str[ndx].hint), fmt, arg); \
  if (!add_opt_enter(list, &ent_str[ndx])) okay = false; }
#define EN_PR_STR(ndx) EN_PR_FMT(ndx, "%s", ent_str[ndx].sdef)
#define EN_PR_INT(ndx) EN_PR_FMT(ndx, "%d", ent_str[ndx].idef)

static gboolean create_main_optmenu(GtkWidget *list) {
  static GtkWidget *main_optmenu_list;
  if (list) main_optmenu_list = list; else list = main_optmenu_list;
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), false);
  gboolean okay = true;
  gtk_compat_list_box_remove_all(GTK_LIST_BOX(list));
  EN_PR_INT(ENT_STR_CYCLES);
  EN_PR_INT(ENT_STR_IVAL);
  if (!add_opt_check(list,  &ent_bool[ENT_BOOL_DNS])) okay = false;
  if (!add_opt_expand(list, &ent_exp[ENT_EXP_INFO]))  okay = false;
  if (!add_opt_expand(list, &ent_exp[ENT_EXP_STAT]))  okay = false;
  if (!add_opt_range(list,  &ent_spn[ENT_SPN_TTL]))   okay = false;
  EN_PR_INT(ENT_STR_QOS);
  EN_PR_STR(ENT_STR_PLOAD);
  EN_PR_INT(ENT_STR_PSIZE);
  if (!add_opt_radio(list, &ent_rad[ENT_RAD_IPV]))    okay = false;
  return okay;
}

static gboolean create_ext_optmenu(GtkWidget *list) {
  static GtkWidget *ext_optmenu_list;
  if (list) ext_optmenu_list = list; else list = ext_optmenu_list;
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), false);
  gboolean okay = true;
  gtk_compat_list_box_remove_all(GTK_LIST_BOX(list));
  if (!add_opt_check(list,      &ent_bool[ENT_BOOL_MN_DARK])) okay = false;
  if (opts.graph) {
    if (!add_opt_check(list,    &ent_bool[ENT_BOOL_GR_DARK])) okay = false;
#ifdef WITH_PLOT
    if (opts.plot)
      if (!add_opt_check(list,  &ent_bool[ENT_BOOL_PL_DARK])) okay = false;
#endif
    if (!add_opt_radio(list,    &ent_rad[ENT_RAD_GRAPH]))     okay = false;
    if (!add_opt_check(list,    &ent_bool[ENT_BOOL_LGND]))    okay = false;
    if (!add_opt_expand(list,   &ent_exp[ENT_EXP_LGFL]))      okay = false;
    if (!add_opt_expand(list,   &ent_exp[ENT_EXP_GREX]))      okay = false;
#ifdef WITH_PLOT
    if (opts.plot) {
      if (!add_opt_expand(list, &ent_exp[ENT_EXP_PLEL]))      okay = false;
      if (!add_opt_range(list,  &ent_spn[ENT_SPN_COLOR]))     okay = false;
      int rotent = opts.rglob ? ENT_SPN_GLOBAL : ENT_SPN_LOCAL;
      if (!add_opt_range(list,  &ent_spn[rotent]))            okay = false;
      if (!add_opt_range(list,  &ent_spn[ENT_SPN_FOV]))       okay = false;
    }
#endif
  }
  EN_PR_INT(ENT_STR_LOGMAX);
  return okay;
}

void init_option_links(void) {
#define INIT_BE_NP(ndx, bname, bprfx) do { \
  ent_bool[ndx].en.name = (bname);         \
  ent_bool[ndx].prefix  = (bprfx);         \
} while (0)
#define INIT_BE_INFO(ndx, bname) INIT_BE_NP((ndx), (bname), OPT_INFO_HDR)
#define INIT_BE_STAT(ndx, bname) INIT_BE_NP((ndx), (bname), OPT_STAT_HDR)
#define INIT_BE_GRLG(ndx, bname) INIT_BE_NP((ndx), (bname), OPT_GRLG_HDR)
#define INIT_BE_GREX(ndx, bname) INIT_BE_NP((ndx), (bname), OPT_GREX_HDR)
#define INIT_BE_PLOT(ndx, bname) INIT_BE_NP((ndx), (bname), OPT_PLOT_HDR)
#define INIT_EE_ND(ndx, ename, edesc) do { \
  ent_exp[ndx].c.en.name = (ename);        \
  ent_exp[ndx].desc      = &(edesc);       \
} while (0)
#define INIT_SE_TD(ndx, sdelim, stitle) do {               \
  ent_spn[ENT_SPN_COLOR].list[ndx].title = (stitle);       \
  ent_spn[ENT_SPN_COLOR].list[ndx].delim = (sdelim);       \
  ent_spn[ENT_SPN_COLOR].list[ndx].aux[0].prfx = (stitle); \
  ent_spn[ENT_SPN_COLOR].list[ndx].aux[1].prfx = (stitle); \
} while (0)
#define INIT_SE_SF(ndx, r1, r2, r3, r4, s1, s2, s3, s4) do { \
  ent_spn[ndx].list[0].aux[r1].prfx = s1; \
  ent_spn[ndx].list[0].aux[r2].prfx = s2; \
  ent_spn[ndx].list[0].aux[r3].prfx = s3; \
  ent_spn[ndx].list[0].aux[r4].prfx = s4; \
} while (0)
  //
  ent_str[ENT_STR_CYCLES].en.name = OPT_CYCLES_HDR;
  ent_str[ENT_STR_IVAL  ].en.name = OPT_IVAL_HDR;
  ent_str[ENT_STR_IVAL  ].en.unit = UNIT_SEC;
  ent_str[ENT_STR_QOS   ].en.name = OPT_QOS_HDR;
  ent_str[ENT_STR_PLOAD ].en.name = OPT_PLOAD_HDR;
  ent_str[ENT_STR_PLOAD ].en.unit = UNIT_HEX;
  ent_str[ENT_STR_PSIZE ].en.name = OPT_PSIZE_HDR;
  ent_str[ENT_STR_LOGMAX].en.name = OPT_LOGMAX_HDR;
  //
  ent_bool[ENT_BOOL_DNS].en.name = OPT_DNS_HDR;
  INIT_BE_INFO(ENT_BOOL_HOST, ELEM_HOST_HEADER);
  INIT_BE_INFO(ENT_BOOL_AS,   ELEM_AS_HEADER  );
  INIT_BE_INFO(ENT_BOOL_CC,   ELEM_CC_HEADER  );
  INIT_BE_INFO(ENT_BOOL_DESC, ELEM_DESC_HEADER);
  INIT_BE_INFO(ENT_BOOL_RT,   ELEM_RT_HEADER  );
  INIT_BE_STAT(ENT_BOOL_LOSS, ELEM_LOSS_HEADER);
  INIT_BE_STAT(ENT_BOOL_SENT, ELEM_SENT_HEADER);
  INIT_BE_STAT(ENT_BOOL_RECV, ELEM_RECV_HEADER);
  INIT_BE_STAT(ENT_BOOL_LAST, ELEM_LAST_HEADER);
  INIT_BE_STAT(ENT_BOOL_BEST, ELEM_BEST_HEADER);
  INIT_BE_STAT(ENT_BOOL_WRST, ELEM_WRST_HEADER);
  INIT_BE_STAT(ENT_BOOL_AVRG, ELEM_AVRG_HEADER);
  INIT_BE_STAT(ENT_BOOL_JTTR, ELEM_JTTR_HEADER);
  ent_bool[ENT_BOOL_MN_DARK].en.name = OPT_MN_DARK_HDR;
  ent_bool[ENT_BOOL_GR_DARK].en.name = OPT_GR_DARK_HDR;
#ifdef WITH_PLOT
  ent_bool[ENT_BOOL_PL_DARK].en.name = OPT_PL_DARK_HDR;
#endif
  ent_bool[ENT_BOOL_LGND].en.name = OPT_LGND_HDR;
  INIT_BE_GRLG(ENT_BOOL_AVJT, GRLG_AVJT_HEADER);
  INIT_BE_GRLG(ENT_BOOL_CCAS, GRLG_CCAS_HEADER);
  INIT_BE_GRLG(ENT_BOOL_LGHN, GRLG_LGHN_HEADER);
  INIT_BE_GREX(ENT_BOOL_MEAN, GREX_MEAN_HEADER);
  INIT_BE_GREX(ENT_BOOL_JRNG, GREX_JRNG_HEADER);
  INIT_BE_GREX(ENT_BOOL_AREA, GREX_AREA_HEADER);
#ifdef WITH_PLOT
  INIT_BE_PLOT(ENT_BOOL_PLBK, PLEL_BACK_HDR);
  INIT_BE_PLOT(ENT_BOOL_PLAX, PLEL_AXIS_HDR);
  INIT_BE_PLOT(ENT_BOOL_PLGR, PLEL_GRID_HDR);
  INIT_BE_PLOT(ENT_BOOL_PLRR, PLEL_ROTR_HDR);
  ent_bool[ENT_BOOL_GLOB].en.name = OPT_GLOB_HDR;
#endif
  //
  INIT_EE_ND(ENT_EXP_INFO, OPT_INFO_HDR, info_desc);
  INIT_EE_ND(ENT_EXP_STAT, OPT_STAT_HDR, stat_desc);
  INIT_EE_ND(ENT_EXP_LGFL, OPT_GRLG_HDR, grlg_desc);
  INIT_EE_ND(ENT_EXP_GREX, OPT_GREX_HDR, grex_desc);
#ifdef WITH_PLOT
  INIT_EE_ND(ENT_EXP_PLEL, OPT_PLOT_HDR, plot_desc);
#endif
  //
  ent_spn[ENT_SPN_TTL   ].c.en.name     = OPT_TTL_HDR;
  ent_spn[ENT_SPN_TTL   ].list[0].title = OPT_TTL_HDR;
  ent_spn[ENT_SPN_TTL   ].list[0].delim = SPN_TTL_DELIM;
#ifdef WITH_PLOT
  ent_spn[ENT_SPN_COLOR ].c.en.name     = OPT_GRAD_HDR;
  INIT_SE_TD(0, SPN_RCOL_DELIM, PLOT_GRAD_COLR);
  INIT_SE_TD(1, SPN_GCOL_DELIM, PLOT_GRAD_COLG);
  INIT_SE_TD(2, SPN_BCOL_DELIM, PLOT_GRAD_COLB);
  ent_spn[ENT_SPN_GLOBAL].c.en.name     = OPT_ROTOR_HDR;
  ent_spn[ENT_SPN_GLOBAL].list[0].title = ROT_AXES;
  INIT_SE_SF(ENT_SPN_GLOBAL,
    ENT_SPN_ROT0, ENT_SPN_ROT1, ENT_SPN_ROT2, ENT_SPN_ANGS,
    ROT_ANGLE_X,  ROT_ANGLE_Y,  ROT_ANGLE_Z,  ROT_STEP);
  ent_spn[ENT_SPN_LOCAL ].c.en.name     = OPT_ROTOR_HDR;
  ent_spn[ENT_SPN_LOCAL ].list[0].title = ROT_ATTITUDE;
  INIT_SE_SF(ENT_SPN_LOCAL,
    ENT_SPN_ROT0, ENT_SPN_ROT1, ENT_SPN_ROT2, ENT_SPN_ANGS,
    ROT_YAW,      ROT_PITCH,    ROT_ROLL,     ROT_STEP);
  ent_spn[ENT_SPN_FOV   ].c.en.name     = OPT_SCALE_HDR;
  ent_spn[ENT_SPN_FOV   ].list[0].title = OPT_FOV_HDR;
  ent_spn[ENT_SPN_FOV   ].list[0].aux[0].prfx  = OPT_FOV_HDR;
#endif
  //
  ent_rad[ENT_RAD_IPV  ].c.en.name  = OPT_IPV_HDR;
  ent_rad[ENT_RAD_IPV  ].map[0].str = OPT_IPVA_HDR;
  ent_rad[ENT_RAD_IPV  ].map[1].str = OPT_IPV4_HDR;
  ent_rad[ENT_RAD_IPV  ].map[2].str = OPT_IPV6_HDR;
  ent_rad[ENT_RAD_GRAPH].c.en.name  = OPT_GRAPH_HDR;
  ent_rad[ENT_RAD_GRAPH].map[0].str = OPT_GR_DOT_HDR;
  ent_rad[ENT_RAD_GRAPH].map[1].str = OPT_GR_LINE_HDR;
  ent_rad[ENT_RAD_GRAPH].map[2].str = OPT_GR_CURVE_HDR;
  //
#undef INIT_BE_NP
#undef INIT_BE_INFO
#undef INIT_BE_STAT
#undef INIT_BE_GRLG
#undef INIT_BE_GREX
#undef INIT_BE_PLOT
#undef INIT_EE_ND
#undef INIT_SE_TD
#undef INIT_SE_SF
}


// pub
//

gboolean option_init(GtkWidget* bar) {
  { const char *icons[] = { ub_theme ? OPT_MAIN_MENU_ICOA : OPT_MAIN_MENU_ICON, OPT_MAIN_MENU_ICOA, NULL};
    g_return_val_if_fail(create_optmenu(bar, icons, OPT_MAINMENU_TIP, create_main_optmenu), false); }
  { const char *icons[] = { OPT_EXT_MENU_ICON, OPT_EXT_MENU_ICOA, NULL};
    g_return_val_if_fail(create_optmenu(bar, icons, OPT_AUXMENU_TIP,  create_ext_optmenu),  false); }
  return true;
}

#define ENT_LOOP_SET(row) { if (GTK_IS_WIDGET(row)) gtk_widget_set_sensitive(row, notrun); }
#define ENT_LOOP_STR(arr) { for (unsigned i = 0; i < G_N_ELEMENTS(arr); i++) ENT_LOOP_SET((arr)[i].box); }
#define ENT_LOOP_EXP(arr) { for (unsigned i = 0; i < G_N_ELEMENTS(arr); i++) if (!(arr)[i].c.atrun) { \
  ENT_LOOP_SET((arr)[i].c.box); if (GTK_IS_TOGGLE_BUTTON((arr)[i].c.arrow)) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON((arr)[i].c.arrow), false); }}

void option_update(void) {
  gboolean notrun = !pinger_state.run;
  ENT_LOOP_STR(ent_str);
  ENT_LOOP_EXP(ent_spn);
  ENT_LOOP_EXP(ent_rad);
}

void option_up_menu_main(void) { if (!opt_silent) { opt_silent = true; create_main_optmenu(NULL); opt_silent = false; }}
void option_up_menu_ext(void)  { if (!opt_silent) { opt_silent = true; create_ext_optmenu(NULL);  opt_silent = false; }}

void option_legend(void) {
  t_ent_bool *en = &ent_bool[ENT_BOOL_LGND];
  gboolean *pbool = EN_BOOLPTR(en);
  if (en->check && pbool) gtk_check_button_set_active(en->check, *pbool);
  toggle_legend();
}

