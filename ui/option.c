
#include "option.h"
#include "parser.h"
#include "pinger.h"
#include "stat.h"
#include "style.h"
#include "notifier.h"
#include "tabs/ping.h"
#include "draw_rel.h"

typedef gboolean (*optmenu_add_items_fn)(GtkWidget *);

typedef struct ent_rad_map {
  int ndx, val;
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
  [ENT_BOOL_DNS]  = { .en = { .type = ENT_BOOL_DNS,  .name = OPT_DNS_HDR }, .pval = &opts.dns },
  [ENT_BOOL_HOST] = { .en = { .type = ENT_BOOL_HOST, .name = ELEM_HOST_HDR },
    .valfn = pingelem_enabler, .valtype = PE_HOST, .prefix = OPT_INFO_HDR },
  [ENT_BOOL_AS]   = { .en = { .type = ENT_BOOL_AS,   .name = ELEM_AS_HDR },
    .valfn = pingelem_enabler, .valtype = PE_AS,   .prefix = OPT_INFO_HDR },
  [ENT_BOOL_CC]   = { .en = { .type = ENT_BOOL_CC,   .name = ELEM_CC_HDR },
    .valfn = pingelem_enabler, .valtype = PE_CC,   .prefix = OPT_INFO_HDR },
  [ENT_BOOL_DESC] = { .en = { .type = ENT_BOOL_DESC, .name = ELEM_DESC_HDR },
    .valfn = pingelem_enabler, .valtype = PE_DESC, .prefix = OPT_INFO_HDR },
  [ENT_BOOL_RT]   = { .en = { .type = ENT_BOOL_RT,   .name = ELEM_RT_HDR },
    .valfn = pingelem_enabler, .valtype = PE_RT,   .prefix = OPT_INFO_HDR },
  [ENT_BOOL_LOSS] = { .en = { .type = ENT_BOOL_LOSS, .name = ELEM_LOSS_HDR},
    .valfn = pingelem_enabler, .valtype = PE_LOSS, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_SENT] = { .en = { .type = ENT_BOOL_SENT, .name = ELEM_SENT_HDR },
    .valfn = pingelem_enabler, .valtype = PE_SENT, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_RECV] = { .en = { .type = ENT_BOOL_RECV, .name = ELEM_RECV_HDR },
    .valfn = pingelem_enabler, .valtype = PE_RECV, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_LAST] = { .en = { .type = ENT_BOOL_LAST, .name = ELEM_LAST_HDR },
    .valfn = pingelem_enabler, .valtype = PE_LAST, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_BEST] = { .en = { .type = ENT_BOOL_BEST, .name = ELEM_BEST_HDR },
    .valfn = pingelem_enabler, .valtype = PE_BEST, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_WRST] = { .en = { .type = ENT_BOOL_WRST, .name = ELEM_WRST_HEADER },
    .valfn = pingelem_enabler, .valtype = PE_WRST, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_AVRG] = { .en = { .type = ENT_BOOL_AVRG, .name = ELEM_AVRG_HEADER },
    .valfn = pingelem_enabler, .valtype = PE_AVRG, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_JTTR] = { .en = { .type = ENT_BOOL_JTTR, .name = ELEM_JTTR_HEADER },
    .valfn = pingelem_enabler, .valtype = PE_JTTR, .prefix = OPT_STAT_HDR },
  [ENT_BOOL_MN_DARK] = { .en = { .type = ENT_BOOL_MN_DARK, .name = OPT_MN_DARK_HDR }, .pval = &opts.darktheme },
  [ENT_BOOL_GR_DARK] = { .en = { .type = ENT_BOOL_GR_DARK, .name = OPT_GR_DARK_HDR }, .pval = &opts.darkgraph },
#ifdef WITH_PLOT
  [ENT_BOOL_PL_DARK] = { .en = { .type = ENT_BOOL_PL_DARK, .name = OPT_PL_DARK_HDR }, .pval = &opts.darkplot },
#endif
  [ENT_BOOL_LGND] = { .en = { .type = ENT_BOOL_LGND, .name = OPT_LGND_HDR }, .pval = &opts.legend },
  [ENT_BOOL_AVJT] = { .en = { .type = ENT_BOOL_AVJT, .name = GRLG_AVJT_HEADER },
    .valfn = graphelem_enabler, .valtype = GE_AVJT, .prefix = OPT_GRLG_HDR },
  [ENT_BOOL_CCAS] = { .en = { .type = ENT_BOOL_CCAS, .name = GRLG_CCAS_HEADER },
    .valfn = graphelem_enabler, .valtype = GE_CCAS, .prefix = OPT_GRLG_HDR },
  [ENT_BOOL_LGHN] = { .en = { .type = ENT_BOOL_LGHN, .name = GRLG_LGHN_HDR },
    .valfn = graphelem_enabler, .valtype = GE_LGHN, .prefix = OPT_GRLG_HDR },
  [ENT_BOOL_MEAN] = { .en = { .type = ENT_BOOL_MEAN, .name = GREX_MEAN_HEADER },
    .valfn = graphelem_enabler, .valtype = GX_MEAN, .prefix = OPT_GREX_HDR },
  [ENT_BOOL_JRNG] = { .en = { .type = ENT_BOOL_JRNG, .name = GREX_JRNG_HEADER },
    .valfn = graphelem_enabler, .valtype = GX_JRNG, .prefix = OPT_GREX_HDR },
  [ENT_BOOL_AREA] = { .en = { .type = ENT_BOOL_AREA, .name = GREX_AREA_HEADER },
    .valfn = graphelem_enabler, .valtype = GX_AREA, .prefix = OPT_GREX_HDR },
#ifdef WITH_PLOT
  [ENT_BOOL_PLBK] = { .en = { .type = ENT_BOOL_PLBK, .name = PLEL_BACK_HDR },
    .valfn = plotelem_enabler, .valtype = D3_BACK, .prefix = OPT_PLOT_HDR },
  [ENT_BOOL_PLAX] = { .en = { .type = ENT_BOOL_PLAX, .name = PLEL_AXIS_HDR },
    .valfn = plotelem_enabler, .valtype = D3_AXIS, .prefix = OPT_PLOT_HDR },
  [ENT_BOOL_PLGR] = { .en = { .type = ENT_BOOL_PLGR, .name = PLEL_GRID_HDR },
    .valfn = plotelem_enabler, .valtype = D3_GRID, .prefix = OPT_PLOT_HDR },
  [ENT_BOOL_PLRR] = { .en = { .type = ENT_BOOL_PLRR, .name = PLEL_ROTR_HDR },
    .valfn = plotelem_enabler, .valtype = D3_ROTR, .prefix = OPT_PLOT_HDR },
  [ENT_BOOL_GLOB] = { .en = { .type = ENT_BOOL_GLOB, .name = OPT_GLOB_HDR }, .pval = &opts.rglob },
#endif
};

t_ent_str ent_str[] = {
  [ENT_STR_CYCLES] = { .len = 6,  .width = 6, .range = { .min = CYCLES_MIN, .max = CYCLES_MAX },
    .en = { .type = ENT_STR_CYCLES, .name = OPT_CYCLES_HDR },
    .pint = &opts.cycles,  .idef = DEF_CYCLES },
  [ENT_STR_IVAL]   = { .len = 4,  .width = 6, .range = { .min = IVAL_MIN,   .max = IVAL_MAX },
    .en = { .type = ENT_STR_IVAL,   .name = OPT_IVAL_HEADER },
    .pint = &opts.timeout, .idef = DEF_TOUT },
  [ENT_STR_QOS]    = { .len = 3,  .width = 6, .range = { .max = QOS_MAX },
    .en = { .type = ENT_STR_QOS,    .name = OPT_QOS_HDR },
    .pint = &opts.qos,     .idef = DEF_QOS },
  [ENT_STR_PLOAD]  = { .len = 48, .width = 6,
    .en = { .type = ENT_STR_PLOAD,  .name = OPT_PLOAD_HEADER },
    .pstr = opts.pad,      .sdef = DEF_PPAD, .slen = sizeof(opts.pad) },
  [ENT_STR_PSIZE]  = { .len = 4,  .width = 6, .range = { .min = PSIZE_MIN,  .max = PSIZE_MAX },
    .en = { .type = ENT_STR_PSIZE,  .name = OPT_PSIZE_HDR },
    .pint = &opts.size,    .idef = DEF_PSIZE },
  [ENT_STR_LOGMAX] = { .len = 3,  .width = 6, .range = { .max = LOGMAX_MAX },
    .en = { .type = ENT_STR_LOGMAX, .name = OPT_LOGMAX_HDR },
    .pint = &opts.logmax,  .idef = DEF_LOGMAX },
};

static t_ent_exp ent_exp[] = {
  [ENT_EXP_INFO] = { .c = {.en = {.type = ENT_EXP_INFO, .name = OPT_INFO_HDR }}, .desc = &info_desc },
  [ENT_EXP_STAT] = { .c = {.en = {.type = ENT_EXP_STAT, .name = OPT_STAT_HDR }}, .desc = &stat_desc },
  [ENT_EXP_LGFL] = { .c = {.en = {.type = ENT_EXP_LGFL, .name = OPT_GRLG_HDR }}, .desc = &grlg_desc },
  [ENT_EXP_GREX] = { .c = {.en = {.type = ENT_EXP_GREX, .name = OPT_GREX_HDR }}, .desc = &grex_desc },
#ifdef WITH_PLOT
  [ENT_EXP_PLEL] = { .c = {.en = {.type = ENT_EXP_PLEL, .name = OPT_PLOT_HDR }}, .desc = &plot_desc },
#endif
};

static void minttl_cb(GtkSpinButton*, t_ent_spn_elem*);
static void maxttl_cb(GtkSpinButton*, t_ent_spn_elem*);
t_minmax opt_mm_ttl = {MINTTL, MAXTTL};

#ifdef WITH_PLOT
#define MIN_COL_VAL 0
#define MAX_COL_VAL 0xff
#define GRAD_COLR "Plot red gradient"
#define GRAD_COLG "Plot green gradient"
#define GRAD_COLB "Plot blue gradient"
#define STR_FROM  "start with"
#define STR_TO    "end with"
static void colgrad_cb(GtkSpinButton*, t_ent_spn_aux*);
t_minmax opt_mm_col = {MIN_COL_VAL, MAX_COL_VAL};
//
#define ROT_ATTITUDE  "Attitude"
#define ROT_ROLL      "Roll"
#define ROT_PITCH     "Pitch"
#define ROT_YAW       "Yaw"
#define ROT_AXES      "Rotation"
#define ROT_ANGLE_X   "Axis X"
#define ROT_ANGLE_Y   "Axis Y"
#define ROT_ANGLE_Z   "Axis Z"
#define ROT_STEP      "Angular step"
static void rotation_cb(GtkSpinButton*, t_ent_spn_aux*);
t_minmax opt_mm_rot = {MIN_VIEW_ANGLE, MAX_VIEW_ANGLE};
t_minmax opt_mm_ang = {1, MAX_VIEW_ANGLE};
#endif

t_ent_spn ent_spn[] = {
  [ENT_SPN_TTL] = { .c = {.en = {.type = ENT_SPN_TTL, .name = OPT_TTL_HDR }}, .list = {
    { .title = OPT_TTL_HDR, .delim = SPN_TTL_DELIM, .bond = true, .kind = MINIMAX_SPIN, .aux = {
      { .cb = G_CALLBACK(minttl_cb), .pval = &opts.min, .def = MINTTL, .mm = &opt_mm_ttl },
      { .cb = G_CALLBACK(maxttl_cb), .pval = &opts.lim, .def = MAXTTL, .mm = &opt_mm_ttl }
  }}}},
#ifdef WITH_PLOT
  [ENT_SPN_COLOR] = { .c = {.en = {.type = ENT_SPN_COLOR, .name = OPT_GRAD_HDR }, .atrun = true},
    .list = {
      { .title = GRAD_COLR, .delim = SPN_RCOL_DELIM, .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.rcol.min, .def = DEF_RCOL_FROM,
          .mm = &opt_mm_col, .prfx = GRAD_COLR, .sfx = STR_FROM },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.rcol.max, .def = DEF_RCOL_TO,
          .mm = &opt_mm_col, .prfx = GRAD_COLR, .sfx = STR_TO }}},
      { .title = GRAD_COLG, .delim = SPN_GCOL_DELIM, .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.gcol.min, .def = DEF_GCOL_FROM,
          .mm = &opt_mm_col, .prfx = GRAD_COLG, .sfx = STR_FROM },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.gcol.max, .def = DEF_GCOL_TO,
          .mm = &opt_mm_col, .prfx = GRAD_COLG, .sfx = STR_TO }}},
      { .title = GRAD_COLB, .delim = SPN_BCOL_DELIM, .kind = MINIMAX_SPIN, .aux = {
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.bcol.min, .def = DEF_BCOL_FROM,
          .mm = &opt_mm_col, .prfx = GRAD_COLB, .sfx = STR_FROM },
        { .cb = G_CALLBACK(colgrad_cb), .pval = &opts.bcol.max, .def = DEF_BCOL_TO,
          .mm = &opt_mm_col, .prfx = GRAD_COLB, .sfx = STR_TO }}},
  }},
  [ENT_SPN_GLOBAL] = { .c = {.en = {.type = ENT_SPN_GLOBAL, .name = OPT_ROTOR_HDR }, .atrun = true},
    .list = {
      { .title = ROT_AXES, .kind = ROTOR_COLUMN, .aux = {
        [ENT_SPN_ROT0] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 0, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_ANGLE_X, .step = &opts.angstep },
        [ENT_SPN_ROT1] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 1, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_ANGLE_Y, .step = &opts.angstep },
        [ENT_SPN_ROT2] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 2, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_ANGLE_Z, .step = &opts.angstep },
        [ENT_SPN_ANGS] = { .cb = G_CALLBACK(rotation_cb), .pval = &opts.angstep, .def = DEF_ANGSTEP,
          .sn = ENT_SPN_GLOBAL, .mm = &opt_mm_ang, .sfx = ROT_STEP },
  }}}},
  [ENT_SPN_LOCAL] = { .c = {.en = {.type = ENT_SPN_LOCAL, .name = OPT_ROTOR_HDR }, .atrun = true},
    .list = {
      { .title = ROT_ATTITUDE, .kind = ROTOR_COLUMN, .aux = {
        [ENT_SPN_ROT0] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 1, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_YAW,   .step = &opts.angstep, .rev = true },
        [ENT_SPN_ROT1] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 2, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_PITCH, .step = &opts.angstep },
        [ENT_SPN_ROT2] = { .cb = G_CALLBACK(rotation_cb), .pval = opts.orient, .pn = 0, .wrap = 360,
          .mm = &opt_mm_rot, .sfx = ROT_ROLL,  .step = &opts.angstep, .rev = true },
        [ENT_SPN_ANGS] = { .cb = G_CALLBACK(rotation_cb), .pval = &opts.angstep, .def = DEF_ANGSTEP,
          .sn = ENT_SPN_LOCAL, .mm = &opt_mm_ang, .sfx = ROT_STEP },
  }}}},
#endif
};

static void graph_type_cb(void);

static t_ent_rad ent_rad[] = {
  [ENT_RAD_IPV] = { .pval = &opts.ipv,
    .c = {.en = {.type = ENT_RAD_IPV, .name = OPT_IPV_HDR }},
    .map = {
      {.ndx = ENT_RAD_IPV, .str = OPT_IPVA_HDR},
      {.ndx = ENT_RAD_IPV, .val = 4, .str = OPT_IPV4_HDR},
      {.ndx = ENT_RAD_IPV, .val = 6, .str = OPT_IPV6_HDR},
    }},
  [ENT_RAD_GRAPH] = { .pval = &opts.graph, .cb = graph_type_cb,
    .c = {.en = {.type = ENT_RAD_GRAPH, .name = OPT_GRAPH_HDR }, .atrun = true },
    .map = {
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_DOT,   .str = OPT_GR_DOT_HDR},
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_LINE,  .str = OPT_GR_LINE_HDR},
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_CURVE, .str = OPT_GR_CURVE_HDR},
    }},
};

static gboolean opt_silent;
#define OPT_NOTIFY(...) { if (!opt_silent) notifier_inform(__VA_ARGS__); }

//

static gboolean check_bool_val(GtkCheckButton *check, t_ent_bool *en, void (*update)(void)) {
  gboolean state = gtk_check_button_get_active(check), re = false;
  if (en) {
    gboolean *pb = EN_BOOLPTR(en);
    if (pb && (*pb != state)) {
      *pb = state; re = true;
      if (update) update();
    }
    if (en->prefix) OPT_NOTIFY("%s: %s %s", en->prefix, en->en.name, onoff(state))
    else            OPT_NOTIFY("%s: %s",                en->en.name, onoff(state));
  }
  return re;
}

static void set_ed_texthint(t_ent_str *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(en->input));
  int *pint = en->pint;
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
  tab_dependent(NULL); STYLE_SET;
  tab_reload_theme();  DRAW_REFRESH_REL;
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
  }
}

static void radio_cb(GtkCheckButton *check, t_ent_rad_map *map) {
  if (!GTK_IS_CHECK_BUTTON(check) || !map) return;
  int ndx = map->ndx; if ((ndx < 0) || (ndx >= G_N_ELEMENTS(ent_rad))) return;
  t_ent_rad *en = &ent_rad[ndx]; int *pval = en->pval;
  if (pval) switch (en->c.en.type) {
    case ENT_RAD_IPV:
    case ENT_RAD_GRAPH: {
      gboolean selected = gtk_check_button_get_active(check);
      if (selected && (map->val != *pval)) {
        *(en->pval) = map->val;
        if (en->cb) en->cb();
        if (map->str) OPT_NOTIFY("%s: %s", en->c.en.name, map->str)
        else          OPT_NOTIFY("%s: %d", en->c.en.name, map->val);
      }
    }
  }
}

static void arrow_cb(GtkWidget *widget, t_ent_exp_common *en) {
  if (!en) return;
  if (GTK_IS_TOGGLE_BUTTON(en->arrow)) {
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
      if (en->en.type == ENT_STR_IVAL) DRAW_REFRESH_REL; // rescale time-axis
    } break;
    case ENT_STR_PLOAD: {
      char *pad = parser_str(got, en->en.name, OPT_TYPE_PAD);
      if (pad) {
        if (en->pstr) g_strlcpy(en->pstr, pad, en->slen);
        OPT_NOTIFY("%s: %s", en->en.name, pad);
        g_free(pad);
      } else set_ed_texthint(en);
    } break;
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
  t_minmax *mm = aux->mm ? aux->mm : &opt_mm_ttl;
  int max = plim ? *plim : mm->max;
  if (pinger_within_range(mm->min, max, got)) {
    OPT_NOTIFY("%s: %d <=> %d", en->title, got, max);
    *pmin = val;
    // then adjust right:range
    double cp_min, cp_max;
    gtk_spin_button_get_range(aux->spin, &cp_min, &cp_max);
    if (got != cp_min) {
      int cp_val = gtk_spin_button_get_value_as_int(aux->spin);
      if (cp_val >= got) // avoid callback triggering
        gtk_spin_button_set_range(aux->spin, got, cp_max);
    }
  } else WARN("out-of-range[%d,%d]: %d", mm->min, max, got);
}

static void maxttl_cb(GtkSpinButton *spin, t_ent_spn_elem *en) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !en) return;
  int *plim = en->aux[1].pval; if (!plim) return;
  int got = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
  if (*plim == got) return;
  t_ent_spn_aux *aux = &en->aux[0];
  int *pmin = aux->pval;
  int min = (pmin ? *pmin : 0) + 1;
  t_minmax *mm = aux->mm ? aux->mm : &opt_mm_ttl;
  if (pinger_within_range(min, mm->max, got)) {
    OPT_NOTIFY("%s: %d <=> %d", en->title, min, got);
    *plim = got;
    // then adjust left:range
    double cp_min, cp_max;
    gtk_spin_button_get_range(aux->spin, &cp_min, &cp_max);
    if (got != cp_max) {
      int cp_val = gtk_spin_button_get_value_as_int(aux->spin);
      if (cp_val <= got) // avoid callback triggering
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(aux->spin), cp_min, got);
    }
  } else WARN("out-of-range[%d,%d]: %d", min, mm->max, got);
}

#ifdef WITH_PLOT
static void colgrad_cb(GtkSpinButton *spin, t_ent_spn_aux *aux) {
  if (!GTK_IS_SPIN_BUTTON(spin) || !aux) return;
  int *pval = aux->pval; if (!pval) return;
  int got = gtk_spin_button_get_value_as_int(spin);
  if (*pval == got) return;
  t_minmax *mm = aux->mm ? aux->mm : &opt_mm_col;
  if ((mm->min <= got) && (got <= mm->max)) {
    *pval = got;
    OPT_NOTIFY("%s: %s %d (0x%02x)", aux->prfx, aux->sfx, got, got);
    plottab_refresh(PL_PARAM_COLOR);
  } else WARN("out-of-range[%d,%d]: %d", mm->min, mm->max, got);
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
  t_minmax *mm = aux->mm ? aux->mm : &opt_mm_rot;
  if ((mm->min <= want) && (want <= mm->max)) {
    OPT_NOTIFY("%s: %d", aux->sfx, want)
    if (aux->sn) {
      *pval = want; t_ent_spn_elem *list = ent_spn[aux->sn].list;
      if (list) for (int i = 0; i < ENT_SPN_ANGS; i++) { // adjust angle entries' step
        GtkSpinButton *sb = list->aux[i].spin;
        if (GTK_IS_SPIN_BUTTON(sb)) gtk_spin_button_set_increments(sb, want, want * 6); }
    } else { *pval = want; set_rotor_n_redraw(step, aux->rev, aux->pn); }
  }
}
#endif

static void graph_type_cb(void) {
  if (!pinger_state.pause) graphtab_update();
  graphtab_refresh();
}

static GtkWidget* label_box(const char *name) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN * 2);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  GtkWidget *label = gtk_label_new(name);
  g_return_val_if_fail(GTK_IS_LABEL(label), box);
  gtk_box_append(GTK_BOX(box), label);
  if (style_loaded) gtk_widget_add_css_class(label, CSS_PAD);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(label, true);
  return box;
}

static GtkWidget* add_labelbox(GtkWidget* list, const char *name) {
  GtkWidget *box = label_box(name);
  if (box) {
    if (GTK_IS_LIST_BOX(list)) gtk_list_box_append(GTK_LIST_BOX(list), box);
    else if (GTK_IS_BOX(list)) gtk_box_append(GTK_BOX(list), box);
  }
  return box;
}

static GtkWidget* add_opt_check(GtkWidget* list, t_ent_bool *en) {
  if (!en) return NULL;
  GtkWidget *box = add_labelbox(list, en->en.name);
  if (box) {
    GtkWidget *check = gtk_check_button_new();
    g_return_val_if_fail(GTK_IS_CHECK_BUTTON(check), box);
    gtk_box_append(GTK_BOX(box), check);
    if (style_loaded) gtk_widget_add_css_class(check, CSS_CHPAD);
    gtk_widget_set_halign(check, GTK_ALIGN_END);
    g_signal_connect(check, EV_TOGGLE, G_CALLBACK(toggle_cb), en);
    en->check = GTK_CHECK_BUTTON(check);
    gboolean *pb = EN_BOOLPTR(en);
    if (pb) gtk_check_button_set_active(en->check, *pb);
  }
  return box;
}

static GtkWidget* add_opt_enter(GtkWidget* list, t_ent_str *en) {
  if (!en) return NULL;
  GtkWidget *box = add_labelbox(list, en->en.name);
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
  GtkWidget *title = label_box(en->en.name);
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
    t_elem_desc *desc = en->desc; type2ndx_fn type2ndx = desc->t2e;
    if (type2ndx) for (int i = desc->mm.min; i <= desc->mm.max; i++) {
      int type = desc->elems[i].type;
      int ndx = type2ndx(type);
      if (ndx < 0) WARN("Unknown %d type", type);
      else add_opt_check(en->c.sub, &ent_bool[ndx]);
    }
  }
  return box;
}

#define SPN_MM_OR_DEF(aux, inc) ((aux).pval ? (*((aux).pval) + (inc)) : (aux).def)
#define SPN_MM_NDX(mm_ndx, inc) ((ndx == mm_ndx) ? (en->aux[mm_ndx].def) : SPN_MM_OR_DEF(en->aux[mm_ndx], inc))

static gboolean add_minmax(GtkWidget *box, t_ent_spn_elem *en, int ndx, int *step, gboolean wrap) {
  gboolean re = false;
  if (GTK_IS_BOX(box) && en) {
    t_minmax *mm0 = en->aux[en->bond ? 0 : ndx].mm;
    t_minmax *mm1 = en->aux[en->bond ? 1 : ndx].mm;
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
        if (en->aux[ndx].cb) g_signal_connect(spin, EV_VAL_CHANGE, en->aux[ndx].cb, en->bond ? (void*)en : &en->aux[ndx]);
        re = true;
     } else WARN_("gtk_spin_button_new_with_range()");
  }}
  return re;
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
    for (int n = 0; (n < G_N_ELEMENTS(en->list)) && elem && elem->title; n++, elem++) {
      GtkWidget *subbox = NULL;
      switch (elem->kind) {
        case MINIMAX_SPIN: subbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN); break;
#ifdef WITH_PLOT
        case ROTOR_COLUMN: subbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);   break;
#endif
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
          for (int i = 0; i < ENT_SPN_ROT_MAX; i++) if (elem->aux[i].sfx) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
            if (!row) continue;
            grey_into_box(row, gtk_label_new(elem->aux[i].sfx), true, true);
            add_minmax(row, elem, i, elem->aux[i].step, elem->aux[i].wrap);
            gtk_box_append(GTK_BOX(subbox), row);
            if ((i >= ENT_SPN_ROT0) && (i <= ENT_SPN_ROT2)) // leave only buttons for rotation spins
              for (GtkWidget *c = gtk_widget_get_first_child(GTK_WIDGET(elem->aux[i].spin));
                GTK_IS_WIDGET(c); c = gtk_widget_get_next_sibling(c))
                  if (GTK_IS_TEXT(c)) gtk_widget_set_visible(c, false);
          } break;
#endif
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
    for (int i = 0; (i < G_N_ELEMENTS(en->map)) && map && map->str; i++, map++) {
      GtkWidget *rb = gtk_check_button_new_with_label(map->str);
      g_return_val_if_fail(GTK_CHECK_BUTTON(rb), box);
      if (style_loaded) gtk_widget_add_css_class(rb, CSS_PAD);
      if (group) gtk_check_button_set_group(GTK_CHECK_BUTTON(rb), GTK_CHECK_BUTTON(group));
      else group = rb;
      if (en->pval) gtk_check_button_set_active(GTK_CHECK_BUTTON(rb), *(en->pval) == map->val);
      g_signal_connect(rb, EV_TOGGLE, G_CALLBACK(radio_cb), map);
      gtk_list_box_append(GTK_LIST_BOX(en->c.sub), rb);
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

static gboolean create_optmenu(GtkWidget *bar, const char **icons, const char *tooltip, optmenu_add_items_fn fn) {
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
    }
#endif
  }
  EN_PR_INT(ENT_STR_LOGMAX);
  return okay;
}


// pub
//

gboolean option_init(GtkWidget* bar) {
  { const char *icons[] = { ub_theme ? OPT_MAIN_MENU_ICOA : OPT_MAIN_MENU_ICON, OPT_MAIN_MENU_ICOA, NULL};
    g_return_val_if_fail(create_optmenu(bar, icons, OPT_MAIN_MENU_TIP, create_main_optmenu), false); }
  { const char *icons[] = { OPT_EXT_MENU_ICON, OPT_EXT_MENU_ICOA, NULL};
    g_return_val_if_fail(create_optmenu(bar, icons, OPT_EXT_MENU_TIP,  create_ext_optmenu),  false); }
  return true;
}

#define ENT_LOOP_SET(row) { if (GTK_IS_WIDGET(row)) gtk_widget_set_sensitive(row, notrun); }
#define ENT_LOOP_STR(arr) { for (int i = 0; i < G_N_ELEMENTS(arr); i++) ENT_LOOP_SET(arr[i].box); }
#define ENT_LOOP_EXP(arr) { for (int i = 0; i < G_N_ELEMENTS(arr); i++) if (!arr[i].c.atrun) { \
  ENT_LOOP_SET(arr[i].c.box); if (GTK_IS_TOGGLE_BUTTON(arr[i].c.arrow)) \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(arr[i].c.arrow), false); }}

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
  gboolean *pb = EN_BOOLPTR(en);
  if (en->check && pb) gtk_check_button_set_active(en->check, *pb);
  toggle_legend();
}

