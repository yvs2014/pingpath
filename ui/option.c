
#include "option.h"
#include "parser.h"
#include "pinger.h"
#include "stat.h"
#include "style.h"
#include "notifier.h"
#include "tabs/ping.h"
#include "tabs/graph.h"

typedef gboolean (*optmenu_add_items_fn)(GtkWidget *);

t_ent_bool ent_bool[ENT_BOOL_MAX] = {
  [ENT_BOOL_DNS]  = { .en = { .typ = ENT_BOOL_DNS,  .name = OPT_DNS_HDR },    .pval = &opts.dns },
  [ENT_BOOL_HOST] = { .en = { .typ = ENT_BOOL_HOST, .name = ELEM_HOST_HDR },  .pval = &statelem[ELEM_HOST].enable, .prefix = OPT_INFO_HDR },
  [ENT_BOOL_AS]   = { .en = { .typ = ENT_BOOL_AS,   .name = ELEM_AS_HDR },    .pval = &statelem[ELEM_AS].enable,   .prefix = OPT_INFO_HDR },
  [ENT_BOOL_CC]   = { .en = { .typ = ENT_BOOL_CC,   .name = ELEM_CC_HDR },    .pval = &statelem[ELEM_CC].enable,   .prefix = OPT_INFO_HDR  },
  [ENT_BOOL_DESC] = { .en = { .typ = ENT_BOOL_DESC, .name = ELEM_DESC_HDR },  .pval = &statelem[ELEM_DESC].enable, .prefix = OPT_INFO_HDR  },
  [ENT_BOOL_RT]   = { .en = { .typ = ENT_BOOL_RT,   .name = ELEM_RT_HDR },    .pval = &statelem[ELEM_RT].enable,   .prefix = OPT_INFO_HDR  },
  [ENT_BOOL_LOSS] = { .en = { .typ = ENT_BOOL_LOSS, .name = ELEM_LOSS_HDRL},  .pval = &statelem[ELEM_LOSS].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_SENT] = { .en = { .typ = ENT_BOOL_SENT, .name = ELEM_SENT_HDR },  .pval = &statelem[ELEM_SENT].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_RECV] = { .en = { .typ = ENT_BOOL_RECV, .name = ELEM_RECV_HDR },  .pval = &statelem[ELEM_RECV].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_LAST] = { .en = { .typ = ENT_BOOL_LAST, .name = ELEM_LAST_HDR },  .pval = &statelem[ELEM_LAST].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_BEST] = { .en = { .typ = ENT_BOOL_BEST, .name = ELEM_BEST_HDR },  .pval = &statelem[ELEM_BEST].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_WRST] = { .en = { .typ = ENT_BOOL_WRST, .name = ELEM_WRST_HDRL }, .pval = &statelem[ELEM_WRST].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_AVRG] = { .en = { .typ = ENT_BOOL_AVRG, .name = ELEM_AVRG_HDRL }, .pval = &statelem[ELEM_AVRG].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_JTTR] = { .en = { .typ = ENT_BOOL_JTTR, .name = ELEM_JTTR_HDRL }, .pval = &statelem[ELEM_JTTR].enable, .prefix = OPT_STAT_HDR  },
  [ENT_BOOL_LGND] = { .en = { .typ = ENT_BOOL_LGND, .name = OPT_LGND_HDR },   .pval = &opts.legend },
};

t_ent_str ent_str[ENT_STR_MAX] = {
  [ENT_STR_CYCLES] = { .len = 6,  .width = 6, .range = { .min = CYCLES_MIN, .max = CYCLES_MAX },
    .en = { .typ = ENT_STR_CYCLES, .name = OPT_CYCLES_HDR },
    .pint = &opts.cycles,  .idef = DEF_CYCLES },
  [ENT_STR_IVAL]   = { .len = 4,  .width = 6, .range = { .min = IVAL_MIN,   .max = IVAL_MAX },
    .en = { .typ = ENT_STR_IVAL,   .name = OPT_IVAL_HDRL },
    .pint = &opts.timeout, .idef = DEF_TOUT },
  [ENT_STR_QOS]    = { .len = 3,  .width = 6, .range = { .max = QOS_MAX },
    .en = { .typ = ENT_STR_QOS,    .name = OPT_QOS_HDR },
    .pint = &opts.qos,     .idef = DEF_QOS },
  [ENT_STR_PLOAD]  = { .len = 48, .width = 6,
    .en = { .typ = ENT_STR_PLOAD,  .name = OPT_PLOAD_HDRL },
    .pstr = opts.pad,      .sdef = DEF_PPAD, .slen = sizeof(opts.pad) },
  [ENT_STR_PSIZE]  = { .len = 4,  .width = 6, .range = { .min = PSIZE_MIN,  .max = PSIZE_MAX },
    .en = { .typ = ENT_STR_PSIZE,  .name = OPT_PSIZE_HDR },
    .pint = &opts.size,    .idef = DEF_PSIZE },
  [ENT_STR_LOGMAX] = { .len = 3,  .width = 6, .range = { .max = LOGMAX_MAX },
    .en = { .typ = ENT_STR_LOGMAX, .name = OPT_LOGMAX_HDR },
    .pint = &opts.logmax,  .idef = DEF_LOGMAX },
};

static t_ent_exp ent_exp[ENT_EXP_MAX] = {
  [ENT_EXP_INFO] = { .c = {.en = {.typ = ENT_EXP_INFO, .name = OPT_INFO_HDR }}, .ndxs = {ENT_BOOL_HOST,
    ENT_BOOL_AS, ENT_BOOL_CC, ENT_BOOL_DESC, ENT_BOOL_RT }},
  [ENT_EXP_STAT] = { .c = {.en = {.typ = ENT_EXP_STAT, .name = OPT_STAT_HDR }}, .ndxs = {ENT_BOOL_LOSS,
    ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR }},
};

static void min_cb(GtkWidget*, t_ent_spn*);
static void max_cb(GtkWidget*, t_ent_spn*);
t_ent_spn ent_spn[ENT_SPN_MAX] = {
  [ENT_SPN_TTL] = { .c = {.en = {.typ = ENT_SPN_TTL, .name = OPT_TTL_HDR }}, .aux = {
    [SPN_AUX_MIN] = {.cb = G_CALLBACK(min_cb), .pval = &opts.min, .def = 1},
    [SPN_AUX_LIM] = {.cb = G_CALLBACK(max_cb), .pval = &opts.lim, .def = MAXTTL}
  }},
};

static void graph_type_cb(void);

static t_ent_rad ent_rad[ENT_RAD_MAX] = {
  [ENT_RAD_IPV] = { .pval = &opts.ipv,
    .c = {.en = {.typ = ENT_RAD_IPV, .name = OPT_IPV_HDR }},
    .map = {
      {.ndx = ENT_RAD_IPV, .str = OPT_IPVA_HDR},
      {.ndx = ENT_RAD_IPV, .val = 4, .str = OPT_IPV4_HDR},
      {.ndx = ENT_RAD_IPV, .val = 6, .str = OPT_IPV6_HDR},
    }},
  [ENT_RAD_GRAPH] = { .pval = &opts.graph, .cb = graph_type_cb,
    .c = {.en = {.typ = ENT_RAD_GRAPH, .name = OPT_GRAPH_HDR }, .atrun = true },
    .map = {
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_NONE,  .str = OPT_GR_NONE_HDR},
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_DOT,   .str = OPT_GR_DOT_HDR},
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_LINE,  .str = OPT_GR_LINE_HDR},
      {.ndx = ENT_RAD_GRAPH, .val = GRAPH_TYPE_CURVE, .str = OPT_GR_CURVE_HDR},
    }},
};

static void check_bool_val(GtkCheckButton *check, t_ent_bool *en, void (*update)(void)) {
  gboolean state = gtk_check_button_get_active(check);
  if (en->pval) if (*(en->pval) != state) {
    *(en->pval) = state;
    if (update) update();
  }
  const gchar *onoff = state ? TOGGLE_ON_HDR : TOGGLE_OFF_HDR;
  if (en->prefix) PP_NOTIFY("%s: %s %s", en->prefix, en->en.name, onoff);
  else PP_NOTIFY("%s %s", en->en.name, onoff);
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

static void toggled_dns(void) {
  stat_reset_cache();
  pinger_update_tabs(NULL);
}

static void toggle_cb(GtkCheckButton *check, t_ent_bool *en) {
  if (!GTK_IS_CHECK_BUTTON(check) || !en) return;
  switch (en->en.typ) {
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
      check_bool_val(check, en, pingtab_vis_cols);
      stat_whois_enabler();
      if (whois_enable) stat_run_whois_resolv();
      break;
    case ENT_BOOL_LGND:
      check_bool_val(check, en, graphtab_toggle_legend);
      break;
  }
}

static void radio_cb(GtkCheckButton *check, t_ent_rad_map *map) {
  if (!map) return;
  g_return_if_fail(GTK_IS_CHECK_BUTTON(check));
  int ndx = map->ndx;
  if ((ndx < 0) || (ndx >= G_N_ELEMENTS(ent_rad[0].map))) return;
  t_ent_rad *en = &ent_rad[ndx];
  switch (en->c.en.typ) {
    case ENT_RAD_GRAPH:
    case ENT_RAD_IPV: {
      gboolean selected = gtk_check_button_get_active(check);
      if (selected) {
        if (map->val != *(en->pval)) {
          *(en->pval) = map->val;
          if (en->cb) en->cb();
          if (map->str) PP_NOTIFY("%s: %s", en->c.en.name, map->str);
          else PP_NOTIFY("%s: %d", en->c.en.name, map->val);
        }
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
  const gchar *got = gtk_editable_get_text(GTK_EDITABLE(input));
  switch (en->en.typ) {
    case ENT_STR_CYCLES:
    case ENT_STR_IVAL:
    case ENT_STR_LOGMAX:
    case ENT_STR_QOS:
    case ENT_STR_PSIZE: {
      int n = parser_int(got, en->en.typ, en->en.name, en->range);
      if (n > 0) {
        if (en->pint) *(en->pint) = n;
        /*extra*/ if (en->en.typ == ENT_STR_IVAL) opts.tout_usec = n * 1000000;
        PP_NOTIFY("%s: %d", en->en.name, n);
      } else set_ed_texthint(en);
    } break;
    case ENT_STR_PLOAD: {
      const char *pad = parser_str(got, en->en.name, PAD_SIZE, STR_RX_PAD);
      if (pad) {
        if (en->pstr) g_strlcpy(en->pstr, pad, en->slen);
        PP_NOTIFY("%s: %s", en->en.name, pad);
      } else set_ed_texthint(en);
    } break;
  }
  DEBUG("%s: %s", en->en.name, got);
}

static void min_cb(GtkWidget *spin, t_ent_spn *en) {
  if (!en) return;
  int *pmin = en->aux[SPN_AUX_MIN].pval;
  if (!pmin) return;
  g_return_if_fail(GTK_IS_SPIN_BUTTON(spin));
  int got = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
  int val = got - 1;
  if (*pmin == val) return;
  int *plim = en->aux[SPN_AUX_LIM].pval;
  int max = plim ? *plim : MAXTTL;
  if (pinger_within_range(1, max, got)) {
    PP_NOTIFY("%s: %d <=> %d", en->c.en.name, got, max);
    *pmin = val;
    // then adjust right:range
    double cp_min, cp_max;
    t_ent_spn_aux *cp = &en->aux[SPN_AUX_LIM];
    gtk_spin_button_get_range(GTK_SPIN_BUTTON(cp->spn), &cp_min, &cp_max);
    if (got != cp_min) {
      int cp_val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cp->spn));
      if (cp_val >= got) { // avoid callback triggering
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(cp->spn), got, cp_max);
      }
    }
  } else WARN("out-of-range[%d,%d]: %d", 1, max, got);
}

static void max_cb(GtkWidget *spin, t_ent_spn *en) {
  if (!en) return;
  int *plim = en->aux[SPN_AUX_LIM].pval;
  if (!plim) return;
  g_return_if_fail(GTK_IS_SPIN_BUTTON(spin));
  int got = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
  if (*plim == got) return;
  int *pmin = en->aux[SPN_AUX_MIN].pval;
  int min = (pmin ? *pmin : 0) + 1;
  if (pinger_within_range(min, MAXTTL, got)) {
    PP_NOTIFY("%s: %d <=> %d", en->c.en.name, min, got);
    *plim = got;
    // then adjust left:range
    double cp_min, cp_max;
    t_ent_spn_aux *cp = &en->aux[SPN_AUX_MIN];
    gtk_spin_button_get_range(GTK_SPIN_BUTTON(cp->spn), &cp_min, &cp_max);
    if (got != cp_max) {
      int cp_val = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cp->spn));
      if (cp_val <= got) { // avoid callback triggering
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(cp->spn), cp_min, got);
      }
    }
  } else WARN("out-of-range[%d,%d]: %d", min, MAXTTL, got);
}

static void graph_type_cb(void) {
  if (opts.graph == GRAPH_TYPE_NONE) graphtab_free();
  if (!pinger_state.pause) graphtab_force_update();
}

static GtkWidget* label_box(const gchar *name) {
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

static GtkWidget* add_labelbox(GtkWidget* list, const gchar *name) {
  GtkWidget *box = label_box(name);
  if (box) gtk_list_box_append(GTK_LIST_BOX(list), box);
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
    if (en->pval) gtk_check_button_set_active(en->check, *(en->pval));
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
    if (style_loaded) {
      gtk_widget_add_css_class(en->arrow, CSS_EXP);
      gtk_widget_add_css_class(en->arrow, CSS_NOFRAME);
    }
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
  if (GTK_IS_BOX(box) && en->c.sub) {
    for (int i = 0; i < G_N_ELEMENTS(en->ndxs); i++) {
      int ndx = en->ndxs[i];
      if (!ndx) break;
      add_opt_check(en->c.sub, &ent_bool[ndx]);
    }
  }
  return box;
}

static int spn_mm_or_def(t_ent_spn_aux *aux, int min, int def) {
  return aux ? (aux->pval ? (*aux->pval + min) : aux->def) : def;
}

static gboolean add_minmax(GtkWidget *box, t_ent_spn *en, int ndx) {
  if (!en || !GTK_IS_BOX(box)) return false;
  int min = (ndx == SPN_AUX_MIN) ? en->aux[SPN_AUX_MIN].def : spn_mm_or_def(&en->aux[SPN_AUX_MIN], 1, 1);
  int max = (ndx == SPN_AUX_LIM) ? en->aux[SPN_AUX_LIM].def : spn_mm_or_def(&en->aux[SPN_AUX_LIM], 0, MAXTTL);
  GtkWidget *spn = gtk_spin_button_new_with_range(min, max, 1);
  g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spn), false);
  gtk_box_append(GTK_BOX(box), spn);
  int *pval = en->aux[ndx].pval;
  if (pval) {
    int val = *pval;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spn), (ndx == SPN_AUX_MIN) ? val + 1 : val);
  }
  en->aux[ndx].spn = spn;
  if (en->aux[ndx].cb) g_signal_connect(spn, EV_VAL_CHANGE, en->aux[ndx].cb, en);
  return true;
}

static void grey_into_box(GtkWidget *box, GtkWidget *widget) {
  if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(widget)) return;
  gtk_widget_set_sensitive(widget, false);
  gtk_box_append(GTK_BOX(box), widget);
}

static GtkWidget* add_opt_range(GtkWidget* list, t_ent_spn *en) {
  if (!en) return NULL;
  GtkWidget *box = add_expand_common(list, &en->c);
  if (GTK_IS_BOX(box) && en->c.sub) {
    GtkWidget *subrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    g_return_val_if_fail(GTK_IS_BOX(subrow), box);
    gtk_list_box_append(GTK_LIST_BOX(en->c.sub), subrow);
    add_minmax(subrow, en, SPN_AUX_MIN);
    grey_into_box(subrow, gtk_image_new_from_icon_name(GO_LEFT_ICON));
    grey_into_box(subrow, gtk_label_new("range")); // "🡸 min max 🡺" "🡸 🡺" "🠔🠖" "←→" "⟷" "🡄🡆" "🢀 🢂"
    grey_into_box(subrow, gtk_image_new_from_icon_name(GO_RIGHT_ICON));
    add_minmax(subrow, en, SPN_AUX_LIM);
  }
  return box;
}

static GtkWidget* add_opt_radio(GtkWidget* list, t_ent_rad *en) {
  if (!en) return NULL;
  GtkWidget *box = add_expand_common(list, &en->c);
  if (GTK_IS_BOX(box) && en->c.sub) {
    GtkWidget *group = NULL;
    for (int i = 0; i < G_N_ELEMENTS(en->map); i++) {
      t_ent_rad_map *map = &en->map[i];
      if (!map->str) break;
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

#define EN_PR_FMT(ndx, fmt, arg) { g_snprintf(ent_str[ndx].hint, sizeof(ent_str[ndx].hint), fmt, arg); \
  if (!add_opt_enter(list, &ent_str[ndx])) okay = false; }
#define EN_PR_STR(ndx) EN_PR_FMT(ndx, "%s", ent_str[ndx].sdef)
#define EN_PR_INT(ndx) EN_PR_FMT(ndx, "%d", ent_str[ndx].idef)

#if GTK_MAJOR_VERSION == 4
#if GTK_MINOR_VERSION < 12
static void gtk_list_box_remove_all(GtkListBox *box) {
  g_return_if_fail(GTK_IS_LIST_BOX(box));
  for (GtkWidget *c; (c = gtk_widget_get_first_child(GTK_WIDGET(box)));) gtk_list_box_remove(box, c);
}
#endif
#endif

static gboolean create_optmenu(GtkWidget *bar, const char *icon, const char *tooltip, optmenu_add_items_fn fn) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  GtkWidget *menu = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(menu), false);
  gboolean okay = true;
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), menu);
  if (icon) gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu), icon);
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

static gboolean create_ping_optmenu(GtkWidget *list) {
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), false);
  gboolean okay = true;
  gtk_list_box_remove_all(GTK_LIST_BOX(list));
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

static gboolean create_graph_optmenu(GtkWidget *list) {
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), false);
  gboolean okay = true;
  gtk_list_box_remove_all(GTK_LIST_BOX(list));
  if (!add_opt_radio(list, &ent_rad[ENT_RAD_GRAPH]))  okay = false;
  if (!add_opt_check(list, &ent_bool[ENT_BOOL_LGND])) okay = false;
  EN_PR_INT(ENT_STR_LOGMAX);
  return okay;
}


// pub
//

inline gboolean option_init(GtkWidget* bar) {
  g_return_val_if_fail(create_optmenu(bar, OPT_PING_MENU_ICON, OPT_MAIN_TOOLTIP, create_ping_optmenu), false);
  if (opts.graph) g_return_val_if_fail(
    create_optmenu(bar, OPT_GRAPH_MENU_ICON, OPT_AUX_TOOLTIP, create_graph_optmenu), false);
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

void option_toggled(int ndx) {
  if ((ndx >= 0) && (ndx < ENT_BOOL_MAX)) {
    t_ent_bool *en = &ent_bool[ndx];
    if (en->check && en->pval) gtk_check_button_set_active(en->check, *(en->pval));
  }
}

