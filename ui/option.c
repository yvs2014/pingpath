
#include "option.h"
#include "common.h"
#include "parser.h"
#include "pinger.h"
#include "stat.h"
#include "style.h"
#include "tabs/ping.h"

#define ENT_BUFF_SIZE 64
#define SUBLIST_MAX 16

enum { SPN_AUX_MIN, SPN_AUX_LIM, SPN_AUX_MAX };

enum { ENT_EXP_NONE, ENT_EXP_INFO, ENT_EXP_STAT, ENT_EXP_MAX };
enum { ENT_SPN_NONE, ENT_SPN_TTL, ENT_SPN_MAX };
enum { ENT_RAD_NONE, ENT_RAD_IPV, ENT_RAD_MAX };

typedef struct ent_ndx {
  int typ;
  const gchar *name;
} t_ent_ndx;

typedef struct ent_str {
  t_ent_ndx en;
  int *pval, def;
  parser_str_fn parser;
  GtkWidget *box, *input;
  int len, width;
  gchar hint[ENT_BUFF_SIZE];
  gchar buff[ENT_BUFF_SIZE];
} t_ent_str;

typedef struct ent_bool {
  t_ent_ndx en;
  bool *pval;
} t_ent_bool;

typedef struct ent_exp_common {
  t_ent_ndx en;
  GtkWidget *arrow, *box, *sub; // aux widgets
} t_ent_exp_common;

typedef struct ent_exp {
  t_ent_exp_common c;
  int ndxs[SUBLIST_MAX];  // 0 terminated indexes, otherwise max
} t_ent_exp;

typedef struct ent_spn_aux {
  int *pval; // indexed: 0 .. lim-1
  int def;   // not-indexed default: 1 .. lim
  GtkWidget *spn;
  GCallback cb;
} t_ent_spn_aux;

typedef struct ent_spn {
  t_ent_exp_common c;
  t_ent_spn_aux aux[SPN_AUX_MAX];
} t_ent_spn;

typedef struct ent_rad_map {
  int ndx, val;
  gchar *str;
} t_ent_rad_map;

typedef struct ent_rad {
  t_ent_exp_common c;
  int *pval;
  t_ent_rad_map map[SUBLIST_MAX]; // 0 str terminated map, otherwise max
} t_ent_rad;

static t_ent_bool ent_bool[ENT_BOOL_MAX] = {
  [ENT_BOOL_DNS]  = { .en = { .typ = ENT_BOOL_DNS,  .name = "DNS" },     .pval = &opts.dns },
  [ENT_BOOL_HOST] = { .en = { .typ = ENT_BOOL_HOST, .name = "Host" },    .pval = &statelem[ELEM_HOST].enable },
  [ENT_BOOL_LOSS] = { .en = { .typ = ENT_BOOL_LOSS, .name = "Loss, %"},  .pval = &statelem[ELEM_LOSS].enable },
  [ENT_BOOL_SENT] = { .en = { .typ = ENT_BOOL_SENT, .name = "Sent" },    .pval = &statelem[ELEM_SENT].enable },
  [ENT_BOOL_RECV] = { .en = { .typ = ENT_BOOL_RECV, .name = "Recv" },    .pval = &statelem[ELEM_RECV].enable },
  [ENT_BOOL_LAST] = { .en = { .typ = ENT_BOOL_LAST, .name = "Last" },    .pval = &statelem[ELEM_LAST].enable },
  [ENT_BOOL_BEST] = { .en = { .typ = ENT_BOOL_BEST, .name = "Best" },    .pval = &statelem[ELEM_BEST].enable },
  [ENT_BOOL_WRST] = { .en = { .typ = ENT_BOOL_WRST, .name = "Worst" },   .pval = &statelem[ELEM_WRST].enable },
  [ENT_BOOL_AVRG] = { .en = { .typ = ENT_BOOL_AVRG, .name = "Average" }, .pval = &statelem[ELEM_AVRG].enable },
  [ENT_BOOL_JTTR] = { .en = { .typ = ENT_BOOL_JTTR, .name = "Jitter" },  .pval = &statelem[ELEM_JTTR].enable },
};

static t_ent_str ent_str[ENT_STR_MAX] = {
  [ENT_STR_CYCLES] = { .len = 6,  .width = 6, .en = { .typ = ENT_STR_CYCLES, .name = "Cycles" },
    .parser = parser_int, .pval = &opts.count,   .def = DEF_COUNT },
  [ENT_STR_IVAL]   = { .len = 4,  .width = 6, .en = { .typ = ENT_STR_IVAL,   .name = "Interval, sec" },
    .parser = parser_int, .pval = &opts.timeout, .def = DEF_TOUT },
  [ENT_STR_QOS]    = { .len = 3,  .width = 6, .en = { .typ = ENT_STR_QOS,    .name = "QoS" },
    .parser = parser_int, .pval = &opts.qos,     .def = DEF_QOS },
  [ENT_STR_PLOAD]  = { .len = 48, .width = 6, .en = { .typ = ENT_STR_PLOAD,  .name = "Payload, hex" }},
  [ENT_STR_PSIZE]  = { .len = 4,  .width = 6, .en = { .typ = ENT_STR_PSIZE,  .name = "Size" },
    .parser = parser_int, .pval = &opts.size,    .def = DEF_PSIZE },
};

static t_ent_exp ent_exp[ENT_EXP_MAX] = {
  [ENT_EXP_INFO] = { .c = {.en = {.typ = ENT_EXP_INFO, .name = "Hop Info"  }}, .ndxs = {ENT_BOOL_HOST}},
  [ENT_EXP_STAT] = { .c = {.en = {.typ = ENT_EXP_STAT, .name = "Statistics"}}, .ndxs = {ENT_BOOL_LOSS,
    ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR},
  },
};

static void min_cb(GtkWidget*, t_ent_spn*);
static void max_cb(GtkWidget*, t_ent_spn*);
static t_ent_spn ent_spn[ENT_SPN_MAX] = {
  [ENT_SPN_TTL] = { .c = {.en = {.typ = ENT_SPN_TTL, .name = "TTL"}}, .aux = {
    [SPN_AUX_MIN] = {.cb = G_CALLBACK(min_cb), .pval = &opts.min, .def = 1},
    [SPN_AUX_LIM] = {.cb = G_CALLBACK(max_cb), .pval = &opts.lim, .def = MAXTTL}
  }},
};

static t_ent_rad ent_rad[ENT_RAD_MAX] = {
  [ENT_RAD_IPV] = { .c = {.en = {.typ = ENT_RAD_IPV, .name = "IP Version №"}}, .pval = &opts.ipv,
    .map = {
      {.ndx = ENT_RAD_IPV, .str = "Auto"},
      {.ndx = ENT_RAD_IPV, .val = 4, .str = "IPv4"},
      {.ndx = ENT_RAD_IPV, .val = 6, .str = "IPv6"}
    },
  },
};


// aux
//

static void check_bool_val(GtkCheckButton *check, t_ent_bool *en, void (*update)(void)) {
  bool state = gtk_check_button_get_active(check);
  if (en->pval) if (*(en->pval) != state) {
    *(en->pval) = state;
    if (update) update();
  }
  LOG("%s: %s", en->en.name, state ? "on" : "off");
}

static void set_ed_texthint(t_ent_str *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(en->input));
  int *pval = en->pval;
  gtk_editable_delete_text(GTK_EDITABLE(en->input), 0, -1);
  if (pval && (*pval != en->def)) {
    g_snprintf(en->buff, sizeof(en->buff), "%d", *pval);
    gtk_editable_set_text(GTK_EDITABLE(en->input), en->buff);
  } else if (en->hint[0])
    gtk_entry_set_placeholder_text(GTK_ENTRY(en->input), en->hint);
}

static void toggle_cb(GtkCheckButton *check, t_ent_bool *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_CHECK_BUTTON(check));
  switch (en->en.typ) {
    case ENT_BOOL_DNS:
      check_bool_val(check, en, pingtab_wrap_update); break;
    case ENT_BOOL_HOST:
    case ENT_BOOL_LOSS:
    case ENT_BOOL_SENT:
    case ENT_BOOL_RECV:
    case ENT_BOOL_LAST:
    case ENT_BOOL_BEST:
    case ENT_BOOL_WRST:
    case ENT_BOOL_AVRG:
    case ENT_BOOL_JTTR:
      check_bool_val(check, en, pingtab_vis_cols); break;
  }
}

static void radio_cb(GtkCheckButton *check, t_ent_rad_map *map) {
  if (!map) return;
  g_return_if_fail(GTK_IS_CHECK_BUTTON(check));
  int ndx = map->ndx;
  if ((ndx < 0) || (ndx >= G_N_ELEMENTS(ent_rad[0].map))) return;
  t_ent_rad *en = &ent_rad[ndx];
  switch (en->c.en.typ) {
    case ENT_RAD_IPV: {
      bool selected = gtk_check_button_get_active(check);
      if (selected) {
        if (map->val != *(en->pval)) {
          *(en->pval) = map->val;
         if (map->val) LOG("%s: %d", en->c.en.name, map->val);
         else LOG("%s: %s", en->c.en.name, map->str);
        }
      }
    }
  }
}

static void arrow_cb(GtkWidget *widget, t_ent_exp_common *en) {
  if (!en) return;
  if (GTK_IS_TOGGLE_BUTTON(en->arrow)) {
    bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(en->arrow));
    g_object_set(G_OBJECT(en->arrow), SUB_MENU_PROP, active ? SUB_MENU_ICON_UP : SUB_MENU_ICON_DOWN, NULL);
    if (GTK_IS_WIDGET(en->sub)) gtk_widget_set_visible(en->sub, active);
    DEBUG("%s: %d", en->en.name, active);
  }
}

static void input_cb(GtkWidget *input, t_ent_str *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(input));
  const gchar *got = gtk_editable_get_text(GTK_EDITABLE(input));
  switch (en->en.typ) {
// ENT_STR_PLOAD
    case ENT_STR_CYCLES:
    case ENT_STR_IVAL:
    case ENT_STR_QOS:
    case ENT_STR_PSIZE: {
      if (!en->parser) return;
      int n = en->parser(got, en->en.typ, en->en.name);
      if (n > 0) {
        if (en->pval) *(en->pval) = n;
        /*extra*/ if (en->en.typ == ENT_STR_IVAL) opts.tout_usec = n * 1000000;
        LOG("%s: %d", en->en.name, n);
      } else set_ed_texthint(en);
    } return;
  }
  LOG("%s: %s", en->en.name, got); // TMP
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
    LOG("%s: %d <=> %d", en->c.en.name, got, max);
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
    LOG("%s: %d <=> %d", en->c.en.name, min, got);
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
    if (en->pval) gtk_check_button_set_active(GTK_CHECK_BUTTON(check), *(en->pval));
    g_signal_connect(check, EV_TOGGLE, G_CALLBACK(toggle_cb), en);
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
    g_object_set(G_OBJECT(en->arrow), SUB_MENU_PROP, SUB_MENU_ICON_DOWN, NULL);
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

static bool add_minmax(GtkWidget *box, t_ent_spn *en, int ndx) {
  if (!en || !GTK_IS_BOX(box)) return false;
  GtkWidget *spn = gtk_spin_button_new_with_range(en->aux[SPN_AUX_MIN].def, en->aux[SPN_AUX_LIM].def, 1);
  g_return_val_if_fail(GTK_IS_SPIN_BUTTON(spn), false);
  gtk_box_append(GTK_BOX(box), spn);
  int *pval = en->aux[ndx].pval;
  if (pval) {
    int val = *pval;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spn), (ndx == SPN_AUX_MIN) ? val + 1 : val);
  }
  en->aux[ndx].spn = spn;
  if (en->aux[ndx].cb) g_signal_connect(spn, EV_SPIN, en->aux[ndx].cb, en);
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
    grey_into_box(subrow, gtk_image_new_from_icon_name(TO_LEFT_ICON));
    grey_into_box(subrow, gtk_label_new("range")); // "🡸 min max 🡺" "🡸 🡺" "🠔🠖" "←→" "⟷" "🡄🡆" "🢀 🢂"
    grey_into_box(subrow, gtk_image_new_from_icon_name(TO_RIGHT_ICON));
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
  if (!add_opt_enter(list, &ent_str[ndx])) re = false; }
#define EN_PR_S(ndx, arg) EN_PR_FMT(ndx, "%s", arg)
#define EN_PR_INT(ndx) EN_PR_FMT(ndx, "%d", ent_str[ndx].def)

static bool create_optmenu(GtkWidget *bar) {
  static GtkWidget *optmenu;
  if (optmenu) return true;
  optmenu = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(optmenu), false);
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(optmenu), OPT_MENU_ICON);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), optmenu);
  GtkWidget *popover = gtk_popover_new();
  g_return_val_if_fail(GTK_IS_POPOVER(popover), false);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(optmenu), popover);
  gtk_popover_present(GTK_POPOVER(popover));
  GtkWidget *list = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(list), false);
  gtk_popover_set_child(GTK_POPOVER(popover), list);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
  gtk_widget_set_halign(list, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(list, false);
  //
  bool re = true;
  gtk_list_box_remove_all(GTK_LIST_BOX(list));
  EN_PR_INT(ENT_STR_CYCLES);
  EN_PR_INT(ENT_STR_IVAL);
  if (!add_opt_check(list,  &ent_bool[ENT_BOOL_DNS])) re = false;
  if (!add_opt_expand(list, &ent_exp[ENT_EXP_INFO]))  re = false;
  if (!add_opt_expand(list, &ent_exp[ENT_EXP_STAT]))  re = false;
  if (!add_opt_range(list,  &ent_spn[ENT_SPN_TTL]))   re = false;
  EN_PR_INT(ENT_STR_QOS);
  EN_PR_S(ENT_STR_PLOAD,  opts.pad);
  EN_PR_INT(ENT_STR_PSIZE);
  if (!add_opt_radio(list, &ent_rad[ENT_RAD_IPV])) re = false;
  //
  return re;
}


// pub
//

bool option_init(GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  if (!create_optmenu(bar)) return false;
  return true;
}

#define ENT_LOOP_SET(row) { if (GTK_IS_WIDGET(row)) gtk_widget_set_sensitive(row, notrun); }
#define ENT_LOOP_STR(arr) { for (int i = 0; i < G_N_ELEMENTS(arr); i++) ENT_LOOP_SET(arr[i].box); }
#define ENT_LOOP_EXP(arr) { for (int i = 0; i < G_N_ELEMENTS(arr); i++) { ENT_LOOP_SET(arr[i].c.box); \
  if (GTK_IS_TOGGLE_BUTTON(arr[i].c.arrow)) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(arr[i].c.arrow), false); }}

void option_update(void) {
  bool notrun = !pinger_state.run;
  ENT_LOOP_STR(ent_str);
  ENT_LOOP_EXP(ent_spn);
  ENT_LOOP_EXP(ent_rad);
}

