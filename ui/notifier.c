
#include "notifier.h"
#include "common.h"
#include "style.h"
#include "pinger.h"
#include "stat.h"

typedef struct nt_extra {
  GtkListBoxRow *row;        // row
  GtkWidget *box;            // box in row
  GtkWidget *elem[LGFL_MAX]; // dash, cc:as, avrg±jttr, cc:as, hopname
} t_nt_extra;

typedef struct notifier {
  GtkWidget *box, *inbox, *reveal;
  guint visible;
  const int typ;
  const gboolean autohide, dyn_css;
  const char *css;
  const int x, y;
  t_nt_extra *extra;
} t_notifier;

static t_nt_extra nt_extra_graph[MAXTTL];

static t_notifier notifier[NT_NDX_MAX] = {
  [NT_MAIN_NDX]  = { .typ = NT_MAIN_NDX,  .autohide = true, .dyn_css = true },
  [NT_GRAPH_NDX] = { .typ = NT_GRAPH_NDX, .css = CSS_LEGEND, .extra = nt_extra_graph,
    .x = GRAPH_RIGHT + MARGIN * 5, .y = GRAPH_TOP + MARGIN * 2 },
};

static gboolean nt_bglight;

t_stat_elem lgndelem[LGFL_MAX] = {
  [LGFL_NO]   = { .enable = true, .name = "" },
  [LGFL_DASH] = { .enable = true, .name = LGFL_DASH_HEADER, .tip = LGFL_DASH_TIP },
  [LGFL_AVJT] = { .enable = true, .name = LGFL_AVJT_HEADER, .tip = LGFL_AVJT_TIP },
  [LGFL_CCAS] = { .enable = true, .name = LGFL_CCAS_HEADER, .tip = LGFL_CCAS_TIP },
  [LGFL_LGHN] = { .enable = true, .name = LGFL_LGHN_HDR,    .tip = LGFL_LGHN_TIP },
};

//

static gboolean nt_set_visible(t_notifier *nt, gboolean visible) {
  if (nt && (nt->visible != visible)) {
    if (GTK_IS_REVEALER(nt->reveal)) gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), visible);
    nt->visible = visible;
  }
  return G_SOURCE_REMOVE;
}

static gboolean nt_hide(t_notifier *nt) { return nt_set_visible(nt, false); }

static void nt_inform(t_notifier *nt, gchar *mesg) {
  if (!nt || !mesg) return;
  if (!GTK_IS_WIDGET(nt->inbox) || !GTK_IS_REVEALER(nt->reveal)) return;
  if (nt->autohide && nt->visible) nt_hide(nt);
  if (GTK_IS_LABEL(nt->inbox)) {
    LOG_(mesg);
    gtk_label_set_text(GTK_LABEL(nt->inbox), mesg);
  }
  if (nt->dyn_css && (nt_bglight == bg_light)) {
    nt_bglight = !bg_light;
    gtk_widget_remove_css_class(nt->box, bg_light ? CSS_LIGHT_BG : CSS_DARK_BG);
    gtk_widget_add_css_class(nt->box, nt_bglight ? CSS_LIGHT_BG : CSS_DARK_BG);
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), true);
  if (nt->autohide) nt->visible = g_timeout_add(AUTOHIDE_IN, (GSourceFunc)nt_hide, nt);
}

static gboolean nt_legend_pos_cb(GtkOverlay *overlay, GtkWidget *widget, GdkRectangle *r, t_notifier *nt) {
  if (GTK_IS_OVERLAY(overlay) && r && nt) {
    if (GTK_IS_WIDGET(widget) /* && gtk_widget_get_resize_needed(widget) */) {
      int unused;
      gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, &unused, NULL, NULL, NULL);
      gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL,   -1, &unused, NULL, NULL, NULL);
    }
    r->x = nt->x; r->y = nt->y;
    return true;
  }
  return false;
}

#define GRL_WARN(what) WARN("graph legend %s failed", what)

static void nt_add_label_and_align(GtkWidget *label, GtkWidget *line, int align, int max, gboolean visible) {
  if (GTK_IS_LABEL(label) && GTK_BOX(line)) {
    gtk_widget_set_visible(label, visible);
    gtk_box_append(GTK_BOX(line), label);
    if (align >= 0) {
      gtk_widget_set_halign(label, align);
      gtk_label_set_xalign(GTK_LABEL(label), align == GTK_ALIGN_END);
      if (max > 0) gtk_label_set_width_chars(GTK_LABEL(label), max);
    }
    if (style_loaded) gtk_widget_add_css_class(label, CSS_LEGEND_TEXT);
  } else GRL_WARN("gtk_label_new()");
}

static void nt_reveal_sets(GtkWidget *reveal, int align, int transition) {
  if (!GTK_IS_REVEALER(reveal)) return;
  gtk_widget_set_halign(reveal, align);
  gtk_widget_set_valign(reveal, align);
  gtk_revealer_set_transition_type(GTK_REVEALER(reveal), transition);
}

#define NT_LG_LABEL(lab, txt, align, chars, visible) { lab = gtk_label_new(txt); \
  nt_add_label_and_align(lab, ex->box, align, chars, visible); }

static GtkWidget* nt_init(GtkWidget *base, t_notifier *nt) {
  if (!nt || !GTK_IS_WIDGET(base)) return NULL;
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(GTK_IS_OVERLAY(over), NULL);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  if (style_loaded) {
    if (nt->css) gtk_widget_add_css_class(box, nt->css);
    if (nt->dyn_css) {
      gtk_widget_add_css_class(box, nt_bglight ? CSS_LIGHT_BG : CSS_DARK_BG);
      gtk_widget_add_css_class(box, CSS_ROUNDED);
    }
  }
  GtkWidget *inbox = NULL;
  switch (nt->typ) {
    case NT_MAIN_NDX: {
      inbox = gtk_label_new(NULL);
      if (GTK_IS_LABEL(inbox) && style_loaded && nt->dyn_css) gtk_widget_add_css_class(inbox, CSS_INVERT);
      else WARN("action notifier %s failed", "gtk_label_new()");
    } break;
    case NT_GRAPH_NDX: {
      inbox = gtk_list_box_new();
      if (GTK_IS_LIST_BOX(inbox)) {
        if (style_loaded) gtk_widget_add_css_class(inbox, CSS_LIGHT_BG);
        for (int i = 0; i < MAXTTL; i++) {
          t_nt_extra *ex = nt->extra ? &(nt->extra[i]) : NULL;
          if (!ex) continue;
          ex->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
          if (GTK_IS_BOX(ex->box)) { // number, color dash, avrg±jttr, cc:as, hopname
            ex->row = line_row_new(ex->box, false);
            if (GTK_IS_LIST_BOX_ROW(ex->row)) {
              gtk_list_box_append(GTK_LIST_BOX(inbox), GTK_WIDGET(ex->row));
              gtk_widget_set_sensitive(GTK_WIDGET(ex->row), false);
              NT_LG_LABEL(ex->elem[LGFL_NO], g_strdup_printf("%d", i + 1), GTK_ALIGN_END, 2, true);
              gchar *col = get_nth_color(i);
              gchar *span = g_strdup_printf(col ? "<span color=\"%s\" font_weight=\"ultrabold\">—</span>" : "—", col);
              NT_LG_LABEL(ex->elem[LGFL_DASH], span, -1, -1, lgndelem[LGFL_DASH].enable);
              if (GTK_IS_WIDGET(ex->elem[LGFL_DASH]) && col) gtk_label_set_use_markup(GTK_LABEL(ex->elem[LGFL_DASH]), true);
              g_free(col); g_free(span);
              NT_LG_LABEL(ex->elem[LGFL_AVJT], NULL, GTK_ALIGN_START, -1, lgndelem[LGFL_AVJT].enable);
              NT_LG_LABEL(ex->elem[LGFL_CCAS], NULL, GTK_ALIGN_START, -1,  lgndelem[LGFL_CCAS].enable);
              NT_LG_LABEL(ex->elem[LGFL_LGHN], NULL, GTK_ALIGN_START, -1, lgndelem[LGFL_LGHN].enable);
            } else WARN("graph legend %s failed", "gtk_list_box_new()");
          } else GRL_WARN("gtk_box_new()");
        }
      } else GRL_WARN("gtk_list_box_new()");
    } break;
  }
  g_return_val_if_fail(GTK_IS_WIDGET(inbox), NULL);
  gtk_widget_set_visible(inbox, true);
  gtk_widget_set_can_focus(inbox, false);
  gtk_widget_set_hexpand(inbox, true);
  gtk_widget_set_halign(inbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(inbox, GTK_ALIGN_CENTER);
  GtkWidget *reveal = gtk_revealer_new();
  g_return_val_if_fail(GTK_IS_REVEALER(reveal), NULL);
  gtk_revealer_set_reveal_child(GTK_REVEALER(reveal), false);
  gtk_widget_set_visible(reveal, true);
  gtk_widget_set_can_focus(reveal, false);
  gtk_widget_set_hexpand(reveal, true);
  if ((nt->x > 0) && (nt->y > 0)) {
    nt_reveal_sets(reveal, GTK_ALIGN_START, GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    g_signal_connect(over, EV_GET_POS, G_CALLBACK(nt_legend_pos_cb), nt);
  } else nt_reveal_sets(reveal, GTK_ALIGN_CENTER, GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT);
  // link widgets together
  gtk_box_append(GTK_BOX(box), inbox);
  gtk_revealer_set_child(GTK_REVEALER(reveal), box);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), reveal);
  gtk_overlay_set_child(GTK_OVERLAY(over), base);
  nt->inbox = inbox; nt->box = box; nt->reveal = reveal;
  return over;
}

static void nt_fill_legend_elem(GtkWidget* label, const gchar *f1, const gchar *f2, const gchar *delim, int *max) {
  if (GTK_IS_LABEL(label)) {
    gchar *str =((f1) && (f1)[0])
      ? g_strdup_printf("%s%s%s", (f2) ? (f2) : "", delim ? delim : "", f1)
      : ((f2) ? g_strdup_printf("%s", f2) : NULL);
    UPDATE_LABEL(label, str);
    if (str) {
      if (max) { int len = g_utf8_strlen(str, MAXHOSTNAME); if (len > *max) *max = len; }
      g_free(str);
    }
  }
}

static void nt_update_legend_width(int typ, int ndx, int max) {
  if ((typ >= 0) && (typ < NT_NDX_MAX) && (ndx >= 0) && (ndx < LGFL_MAX)) {
    t_nt_extra *ex = notifier[typ].extra;
    if (ex) for (int i = 0; i < MAXTTL; i++, ex++)
      if (GTK_IS_LABEL(ex->elem[ndx])) gtk_label_set_width_chars(GTK_LABEL(ex->elem[ndx]), max);
  }
}


// pub
//

inline GtkWidget* notifier_init(int ndx, GtkWidget *base) {
  nt_bglight = !bg_light;
  return ((ndx >= 0) && (ndx < NT_NDX_MAX)) ? nt_init(base, &notifier[ndx]) : NULL;
}

void notifier_inform(int ndx, const gchar *fmt, ...) {
  if ((ndx < 0) || (ndx >= NT_NDX_MAX)) return;
  va_list ap;
  va_start(ap, fmt);
  gchar *str = g_strdup_vprintf(fmt, ap);
  if (str) { cli ? CLILOG(str) : nt_inform(&notifier[ndx], str); g_free(str); }
  va_end(ap);
}

inline gboolean notifier_get_visible(int ndx) {
  return ((ndx >= 0) && (ndx < NT_NDX_MAX)) ? notifier[ndx].visible : true;
}

inline void notifier_set_visible(int ndx, gboolean visible) {
  if ((ndx >= 0) && (ndx < NT_NDX_MAX)) nt_set_visible(&notifier[ndx], visible);
}

#define NT_LG_ELEM_VIS(ndx, extra_cond) { if (GTK_IS_WIDGET(ex->elem[ndx])) \
  gtk_widget_set_visible(ex->elem[ndx], vis && lgndelem[ndx].enable && extra_cond); }

void notifier_vis_rows(int ndx, int max) {
  static int nt_known_max[NT_NDX_MAX];
  if ((ndx >= 0) && (ndx < NT_NDX_MAX)) {
    t_nt_extra *ex = notifier[ndx].extra;
    if (max < 0) max = nt_known_max[ndx];
    else nt_known_max[ndx] = max;
    if (ex) for (int i = 0; i < MAXTTL; i++, ex++) {
      gboolean vis = (i >= opts.min) && (i < max);
      if (GTK_IS_WIDGET(ex->row)) gtk_widget_set_visible(GTK_WIDGET(ex->row), vis);
      if (GTK_IS_WIDGET(ex->box)) gtk_widget_set_visible(ex->box, vis);
      NT_LG_ELEM_VIS(LGFL_DASH, true);
      NT_LG_ELEM_VIS(LGFL_AVJT, true);
      NT_LG_ELEM_VIS(LGFL_CCAS, opts.whois);
      NT_LG_ELEM_VIS(LGFL_LGHN, true);
    }
  }
}

#define LIMIT_BY_NL(str) { if (str) { char *nl = strchr(str, '\n'); if (nl) *nl = 0; }}

#define LGFL_CHK_MAX(ndx, max) { if (max != nt_lgfl_max[ndx]) { \
  nt_lgfl_max[ndx] = max; nt_update_legend_width(NT_GRAPH_NDX, ndx, max); }}

void notifier_legend_update(int ndx) {
  static int nt_lgfl_max[LGFL_MAX];
  if ((ndx >= 0) && (ndx < NT_NDX_MAX)) {
    t_nt_extra *ex = notifier[ndx].extra;
    if (ex) {
      int aj_max = -1, ca_max = -1;
      for (int i = 0; i < hops_no; i++, ex++) {
        t_stat_graph data = stat_graph_data_at(i);
        if (data.name) {
          LIMIT_BY_NL(data.name);
          if (GTK_IS_LABEL(ex->elem[LGFL_LGHN])) UPDATE_LABEL(ex->elem[LGFL_LGHN], data.name);
        }
        LIMIT_BY_NL(data.as); LIMIT_BY_NL(data.cc);
        nt_fill_legend_elem(ex->elem[LGFL_AVJT], data.jt, data.av, "±", &aj_max);
        nt_fill_legend_elem(ex->elem[LGFL_CCAS], data.as, data.cc, ":", &ca_max);
      }
      LGFL_CHK_MAX(LGFL_AVJT, aj_max);
      LGFL_CHK_MAX(LGFL_CCAS, ca_max);
    }
  }
}

