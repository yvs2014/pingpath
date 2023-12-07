
#include "option.h"
#include "pinger.h"
#include "stat.h"
#include "style.h"
#include "tabs/ping.h"

#define ENT_BUFF_SIZE 64
#define SUBLIST_MAX 16

typedef struct ent_ndx {
  int typ;
  const gchar *name;
} t_ent_ndx;

typedef struct ent_str {
  t_ent_ndx en;
  GtkWidget *box, *input;
  int len, width;
  gchar hint[ENT_BUFF_SIZE];
  gchar buff[ENT_BUFF_SIZE];
} t_ent_str;

typedef struct ent_bool {
  t_ent_ndx en;
  bool *pval;
} t_ent_bool;

typedef struct ent_exp {
  t_ent_ndx en;
  int ndxs[SUBLIST_MAX];  // 0 terminated, otherwise max
  GtkWidget *arrow, *sub; // aux widgets
} t_ent_exp;

enum { ENT_STR_NONE, ENT_STR_CYCLES, ENT_STR_IVAL, ENT_STR_QOS, ENT_STR_PLOAD, ENT_STR_PSIZE, ENT_STR_MAX };
enum { ENT_EXP_NONE, ENT_EXP_INFO, ENT_EXP_STAT, ENT_EXP_MAX };

static t_ent_bool ent_bool[ENT_BOOL_MAX] = {
  [ENT_BOOL_DNS]  = { .en = { .typ = ENT_BOOL_DNS,  .name = "DNS" },     .pval = &ping_opts.dns },
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
  [ENT_STR_CYCLES] = { .len = 6,  .width = 6, .en = { .typ = ENT_STR_CYCLES, .name = "Cycles" }},
  [ENT_STR_IVAL]   = { .len = 2,  .width = 6, .en = { .typ = ENT_STR_IVAL,   .name = "Interval, sec" }},
  [ENT_STR_QOS]    = { .len = 3,  .width = 6, .en = { .typ = ENT_STR_QOS,    .name = "QoS" }},
  [ENT_STR_PLOAD]  = { .len = 48, .width = 6, .en = { .typ = ENT_STR_PLOAD,  .name = "Payload, hex" }},
  [ENT_STR_PSIZE]  = { .len = 4,  .width = 6, .en = { .typ = ENT_STR_PSIZE,  .name = "Size" }},
};

static t_ent_exp ent_exp[ENT_EXP_MAX] = {
  [ENT_EXP_INFO] = { .en = { .typ = ENT_EXP_INFO, .name = "Hop Info" }, .ndxs = {ENT_BOOL_HOST}},
  [ENT_EXP_STAT] = { .en = { .typ = ENT_EXP_STAT, .name = "Statistics" }, .ndxs = {ENT_BOOL_LOSS,
    ENT_BOOL_SENT, ENT_BOOL_RECV, ENT_BOOL_LAST, ENT_BOOL_BEST, ENT_BOOL_WRST, ENT_BOOL_AVRG, ENT_BOOL_JTTR},
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

static void arrow_cb(GtkWidget *widget, t_ent_exp *en) {
  if (!en) return;
  if (GTK_IS_TOGGLE_BUTTON(en->arrow)) {
    bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(en->arrow));
    g_object_set(G_OBJECT(en->arrow), SUB_MENU_PROP, active ? SUB_MENU_ICON_UP : SUB_MENU_ICON_DOWN, NULL);
    if (GTK_IS_WIDGET(en->sub)) gtk_widget_set_visible(GTK_WIDGET(en->sub), active);
  }
  DEBUG("%s: %s: %d", en->en.name, EV_TOGGLE, active);
}

static void input_cb(GtkWidget *input, t_ent_ndx *en) {
  if (!en) return;
  g_return_if_fail(GTK_IS_EDITABLE(input));
//  const gchar *got = gtk_editable_get_text(GTK_EDITABLE(input));
//  gchar *valid = valid_input(en->en.ndx, got);
  DEBUG("%s: %s: %s", en->name, EV_ACTIVE, got);
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

static GtkWidget* add_rl(GtkWidget* list, const gchar *name) {
  GtkWidget *box = label_box(name);
  if (box) gtk_list_box_append(GTK_LIST_BOX(list), box);
  return box;
}

static GtkWidget* add_rlc(GtkWidget* list, t_ent_bool *en) {
  if (!en) return NULL;
  GtkWidget *box = add_rl(list, en->en.name);
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

static GtkWidget* add_rle(GtkWidget* list, t_ent_str *en) {
  if (!en) return NULL;
  GtkWidget *box = add_rl(list, en->en.name);
  if (box) {
    en->box = box;
    en->input = gtk_entry_new();
    g_return_val_if_fail(GTK_IS_ENTRY(en->input), box);
    gtk_box_append(GTK_BOX(box), en->input);
    if (style_loaded) gtk_widget_add_css_class(en->input, CSS_NOPAD);
    gtk_widget_set_halign(en->input, GTK_ALIGN_END);
    gtk_entry_set_alignment(GTK_ENTRY(en->input), 0.99);
    gtk_entry_set_max_length(GTK_ENTRY(en->input), en->len);
    if (en->hint[0]) gtk_entry_set_placeholder_text(GTK_ENTRY(en->input), en->hint);
    gtk_editable_set_editable(GTK_EDITABLE(en->input), true);
    gtk_editable_set_max_width_chars(GTK_EDITABLE(en->input), en->width);
    g_signal_connect(en->input, EV_ACTIVE, G_CALLBACK(input_cb), &en->en);
  }
  return box;
}

static GtkWidget* add_rlx(GtkWidget* list, t_ent_exp *en) {
  if (!en) return false;
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
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
  gtk_widget_set_visible(en->sub, false);
  gtk_box_append(GTK_BOX(box), GTK_WIDGET(en->sub));
  for (int i = 0; i < G_N_ELEMENTS(en->ndxs); i++) {
    int ndx = en->ndxs[i];
    if (!ndx) break;
    add_rlc(en->sub, &ent_bool[ndx]);
  }
  return box;
}

#define EN_PR_FMT(ndx, fmt, arg) { g_snprintf(ent_str[ndx].hint, sizeof(ent_str[ndx].hint), fmt, arg); \
  add_rle(list, &ent_str[ndx]); }
#define EN_PR_D(ndx, arg) EN_PR_FMT(ndx, "%d", arg)
#define EN_PR_S(ndx, arg) EN_PR_FMT(ndx, "%s", arg)

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
  gtk_list_box_remove_all(GTK_LIST_BOX(list));
  EN_PR_D(ENT_STR_CYCLES, ping_opts.count);
  EN_PR_D(ENT_STR_IVAL,   ping_opts.timeout);
  add_rlc(list, &ent_bool[ENT_BOOL_DNS]);
  add_rlx(list, &ent_exp[ENT_EXP_INFO]);
  add_rlx(list, &ent_exp[ENT_EXP_STAT]);
  add_rl(list, "TTL"); // app.opt_ttl
  EN_PR_D(ENT_STR_QOS,   ping_opts.qos);
  EN_PR_S(ENT_STR_PLOAD, ping_opts.pad);
  EN_PR_D(ENT_STR_PSIZE, ping_opts.size);
  add_rl(list, "IP Version №"); // app.opt_ipv
  //
  return true;
}


// pub
//

bool option_init(GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  if (!create_optmenu(bar)) return false;
  option_update();
  return true;
}

void option_update(void) {
  bool notrun = !pinger_state.run;
  for (int i = 0; i < G_N_ELEMENTS(ent_str); i++) {
    GtkWidget *row = ent_str[i].box;
    if (GTK_IS_WIDGET(row)) gtk_widget_set_sensitive(row, notrun);
  }
}

