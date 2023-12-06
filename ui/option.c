
#include "option.h"
#include "pinger.h"
#include "style.h"

#define OPT_MENU_ICON "document-properties-symbolic"
#define ENT_BUFF_SIZE 64

typedef struct ent_ndx {
  GtkWidget *row, *input;
  const gchar *name;
  int ndx, len, width;
  gchar hint[ENT_BUFF_SIZE];
  gchar buff[ENT_BUFF_SIZE];
} t_ent_ndx;

enum { CH_NDX_DNS };
enum { ENT_NDX_CYCLES, ENT_NDX_IVAL, ENT_NDX_QOS, ENT_NDX_PLOAD, ENT_NDX_PSIZE, ENT_NDX_MAX };

static GtkWidget *optlist;

static t_ent_ndx ent_ndx[ENT_NDX_MAX] = {
  [ENT_NDX_CYCLES] = { .len = 6,  .width = 6, .ndx = ENT_NDX_CYCLES, .name = "Cycles" },
  [ENT_NDX_IVAL]   = { .len = 2,  .width = 6, .ndx = ENT_NDX_IVAL,   .name = "Interval, sec" },
  [ENT_NDX_QOS]    = { .len = 3,  .width = 6, .ndx = ENT_NDX_QOS,    .name = "QoS",  },
  [ENT_NDX_PLOAD]  = { .len = 48, .width = 6, .ndx = ENT_NDX_PLOAD,  .name = "Payload, hex" },
  [ENT_NDX_PSIZE]  = { .len = 4,  .width = 6, .ndx = ENT_NDX_PSIZE,  .name = "Size" },
};

// aux
//

static void chb_activate(GtkCheckButton* check, gpointer p) {
  g_return_if_fail(GTK_IS_CHECK_BUTTON(check));
  g_assert(p);
  const int *ndx = p;
  switch (*ndx) {
    case CH_NDX_DNS: {
      ping_opts.dns = gtk_check_button_get_active(check);
      LOG("%s: %s", "DNS", ping_opts.dns ? "on" : "off");
      break;
    }
  }
}

static void ent_activate(GtkWidget *widget, t_ent_ndx *en) {
  g_return_if_fail(GTK_IS_EDITABLE(en->input));
//  gchar *target = valid_target(gtk_editable_get_text(GTK_EDITABLE(en->input)));
  LOG("\"%s\" got: %s", en->name, gtk_editable_get_text(GTK_EDITABLE(en->input)));
}

static GtkWidget* add_rl(GtkWidget* list, const gchar *name) {
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN * 2);
  g_return_val_if_fail(GTK_IS_BOX(row), NULL);
  GtkWidget *label = gtk_label_new(name);
  g_return_val_if_fail(GTK_IS_LABEL(label), NULL);
  if (style_loaded) gtk_widget_add_css_class(label, CSS_PAD);
  gtk_widget_set_halign(label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(label, true);
  gtk_box_append(GTK_BOX(row), label);
  gtk_list_box_append(GTK_LIST_BOX(list), row);
  return row;
}

static bool add_rlc(GtkWidget* list, const gchar *name, int *ndx, gboolean state) {
  GtkWidget *row = add_rl(list, name); g_return_val_if_fail(GTK_IS_BOX(row), false);
  GtkWidget *check = gtk_check_button_new();
  g_return_val_if_fail(GTK_IS_CHECK_BUTTON(check), false);
  if (style_loaded) gtk_widget_add_css_class(check, CSS_CHPAD);
  gtk_widget_set_halign(check, GTK_ALIGN_END);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(check), state);
  gtk_box_append(GTK_BOX(row), check);
  g_signal_connect(check, "toggled", G_CALLBACK(chb_activate), ndx);
  return true;
}

static bool add_rle(GtkWidget* list, t_ent_ndx *en) {
  en->row = add_rl(list, en->name); g_return_val_if_fail(GTK_IS_BOX(en->row), false);
  g_return_val_if_fail(en->row != NULL, false);
  en->input = gtk_entry_new();
  g_return_val_if_fail(GTK_IS_ENTRY(en->input), false);
  if (style_loaded) gtk_widget_add_css_class(en->input, CSS_NOPAD);
  gtk_widget_set_halign(en->input, GTK_ALIGN_END);
  gtk_entry_set_alignment(GTK_ENTRY(en->input), 1);
  gtk_entry_set_max_length(GTK_ENTRY(en->input), en->len);
  if (en->hint[0]) gtk_entry_set_placeholder_text(GTK_ENTRY(en->input), en->hint);
  gtk_editable_set_editable(GTK_EDITABLE(en->input), true);
  gtk_editable_set_max_width_chars(GTK_EDITABLE(en->input), en->width);
  gtk_box_append(GTK_BOX(en->row), en->input);
  g_signal_connect(en->input, "activate", G_CALLBACK(ent_activate), en);
  return true;
}

#define EN_PR_FMT(ndx, fmt, arg) { \
  g_snprintf(ent_ndx[ndx].hint, sizeof(ent_ndx[ndx].hint), fmt, arg); \
  g_return_val_if_fail(add_rle(optlist, &ent_ndx[ndx]), false); \
}
#define EN_PR_D(ndx, arg) EN_PR_FMT(ndx, "%d", arg)
#define EN_PR_S(ndx, arg) EN_PR_FMT(ndx, "%s", arg)

static bool create_list(GtkWidget *bar) {
  static int dns_ndx = CH_NDX_DNS;
  if (!optlist) optlist = gtk_list_box_new();
  g_return_val_if_fail(GTK_IS_LIST_BOX(optlist), false);
  gtk_widget_set_halign(optlist, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(optlist, false);
  gtk_list_box_remove_all(GTK_LIST_BOX(optlist));
  GtkWidget *button = gtk_menu_button_new();
  g_return_val_if_fail(GTK_IS_MENU_BUTTON(button), false);
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), OPT_MENU_ICON);
  gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), button);
  GtkWidget *popover = gtk_popover_new();
  g_return_val_if_fail(GTK_IS_POPOVER(popover), false);
  //
  EN_PR_D(ENT_NDX_CYCLES, ping_opts.count);
  EN_PR_D(ENT_NDX_IVAL,   ping_opts.timeout);
  //
  g_return_val_if_fail(add_rlc(optlist, "DNS", &dns_ndx, ping_opts.dns), false);
  g_return_val_if_fail(add_rl(optlist, "Hop Info"),   false); // app.opt_hopinfo
  g_return_val_if_fail(add_rl(optlist, "Statistics"), false); // app.opt_stat
  g_return_val_if_fail(add_rl(optlist, "TTL"),        false); // app.opt_ttl
  //
  EN_PR_D(ENT_NDX_QOS,   ping_opts.qos);
  EN_PR_S(ENT_NDX_PLOAD, ping_opts.pad);
  EN_PR_D(ENT_NDX_PSIZE, ping_opts.size);
  //
  g_return_val_if_fail(add_rl(optlist, "IP Version №"),  false); // app.opt_ipv
  //
  gtk_popover_set_child(GTK_POPOVER(popover), optlist);
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(optlist), GTK_SELECTION_NONE);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
  gtk_popover_present(GTK_POPOVER(popover));
  return true;
}


// pub
//

bool option_init(GtkWidget* bar) {
  g_return_val_if_fail(GTK_IS_HEADER_BAR(bar), false);
  if (!create_list(bar)) return false;
  option_update();
  return true;
}

void option_update(void) {
  bool notrun = !pinger_state.run;
  for (int i = 0; i < G_N_ELEMENTS(ent_ndx); i++) {
    GtkWidget *row = ent_ndx[i].row;
    if (GTK_IS_WIDGET(row)) gtk_widget_set_sensitive(row, notrun);
  }
}

