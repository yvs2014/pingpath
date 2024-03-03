
#include "notifier.h"
#include "style.h"
#include "pinger.h"
#include "stat.h"
#include "tabs/graph.h"

#define DASH_CHAR "—"
#define LGND_ROW_DEF_STATE true
#define LGND_DIS_MARK_FMT "<span strikethrough='true' strikethrough_color='#ff0000'>%d</span>"
#define LGND_DASH_DEF_FMT "<span weight='800'>%s</span>"
#define LGND_DASH_COL_FMT "<span weight='800' color='%s'>%s</span>"
#define LGND_LINE_TOGGLE_TIP "show/hide graph #%d"

#define GL_FAIL(what) FAILX("graph legend", what)

typedef struct nt_extra {
  GtkListBoxRow *row;        // row
  GtkWidget *box;            // box in row
  GtkWidget *elem[GRLG_MAX]; // dash, cc:as, avrg±jttr, cc:as, hopname
} t_nt_extra;

typedef struct notifier {
  GtkWidget *box, *inbox, *reveal;
  guint visible;
  const int type;
  const gboolean autohide, dyncss;
  const char *defcss, *colcss;
#ifdef WITH_DND
  int x, y, dx, dy;
#else
  const int x, y;
#endif
  t_nt_extra *extra;
} t_notifier;

static t_nt_extra nt_extra_graph[MAXTTL];

static t_notifier notifier[NT_NDX_MAX] = {
  [NT_MAIN_NDX]  = { .type = NT_MAIN_NDX,  .autohide = true, .dyncss = true, .defcss = CSS_ROUNDED },
  [NT_GRAPH_NDX] = { .type = NT_GRAPH_NDX, .extra = nt_extra_graph,
    .defcss = CSS_LEGEND, .colcss = CSS_LEGEND_COL,
    .x = GRAPH_RIGHT + MARGIN * 5, .y = GRAPH_TOP + MARGIN * 2 },
};

//

static gboolean nt_set_visible(t_notifier *nt, gboolean visible) {
  if (!atquit && nt && (nt->visible != visible)) {
    if (GTK_IS_REVEALER(nt->reveal)) gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), visible);
    nt->visible = visible;
  }
  return G_SOURCE_REMOVE;
}

static gboolean nt_hide(t_notifier *nt) { return nt_set_visible(nt, false); }

static void nt_inform(t_notifier *nt, gchar *mesg) {
  static int last_known_bg;
  if (!nt || !mesg) return;
  if (!GTK_IS_WIDGET(nt->inbox) || !GTK_IS_REVEALER(nt->reveal)) return;
  if (nt->autohide && nt->visible) nt_hide(nt);
  if (GTK_IS_LABEL(nt->inbox)) {
    LOG_(mesg);
    gtk_label_set_text(GTK_LABEL(nt->inbox), mesg);
  }
  if (nt->dyncss && (nt_dark != last_known_bg)) {
    gtk_widget_remove_css_class(nt->box, last_known_bg ? CSS_BGONDARK : CSS_BGONLIGHT);
    last_known_bg = nt_dark;
    gtk_widget_add_css_class(nt->box, last_known_bg ? CSS_BGONDARK : CSS_BGONLIGHT);
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), true);
  if (nt->autohide) nt->visible = g_timeout_add_seconds(AUTOHIDE_IN, (GSourceFunc)nt_hide, nt);
}

static gboolean nt_legend_pos_cb(GtkOverlay *overlay, GtkWidget *widget, GdkRectangle *r, t_notifier *nt) {
  if (GTK_IS_OVERLAY(overlay) && r && nt) {
    if (GTK_IS_WIDGET(widget) /* && gtk_widget_get_resize_needed(widget) */) {
      gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, NULL, NULL, NULL);
      gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL,   -1, NULL, NULL, NULL, NULL);
    }
    r->x = nt->x; r->y = nt->y;
    return true;
  }
  return false;
}

static void nt_add_lgelem(GtkWidget *widget, GtkWidget *box, int align, int max, gboolean visible) {
  if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(widget)) { GL_FAIL("gtk_widget_new()"); return; }
  gtk_widget_set_visible(widget, visible);
  gtk_box_append(GTK_BOX(box), widget);
  if (GTK_IS_LABEL(widget) && (align >= 0)) {
    gtk_widget_set_halign(widget, align);
    gtk_label_set_xalign(GTK_LABEL(widget), align == GTK_ALIGN_END);
    if (max > 0) gtk_label_set_width_chars(GTK_LABEL(widget), max);
  }
  if (style_loaded) gtk_widget_add_css_class(widget, CSS_LEGEND_TEXT);
}

static void nt_reveal_sets(GtkWidget *reveal, int align, int transition) {
  if (!GTK_IS_REVEALER(reveal)) return;
  gtk_widget_set_halign(reveal, align);
  gtk_widget_set_valign(reveal, align);
  gtk_revealer_set_transition_type(GTK_REVEALER(reveal), transition);
}

static gchar* nb_lgnd_nth_state(int n, gboolean enable) {
  static gchar nb_lgnd_state_buff[100]; n++;
  if (enable) g_snprintf(nb_lgnd_state_buff, sizeof(nb_lgnd_state_buff), "%d", n);
  else g_snprintf(nb_lgnd_state_buff, sizeof(nb_lgnd_state_buff), LGND_DIS_MARK_FMT, n);
  return nb_lgnd_state_buff;
}

static void nt_lgnd_row_cb(GtkListBox* self, GtkListBoxRow* row, gpointer data) {
  if (GTK_IS_LIST_BOX_ROW(row)) {
    GtkWidget *box = gtk_list_box_row_get_child(row);
    if (GTK_IS_BOX(box)) {
      GtkWidget *no = gtk_widget_get_first_child(box);
      if (GTK_IS_LABEL(no)) {
        const gchar *txt = gtk_label_get_text(GTK_LABEL(no));
        if (txt) {
          int n = atoi(txt); n--;
          if ((n >= opts.min) && (n < opts.lim)) {
            gboolean en = !gtk_list_box_row_get_selectable(row);
            if (en) lgnd_excl_mask &= ~(en << n); else lgnd_excl_mask |= (!en << n);
            gtk_label_set_text(GTK_LABEL(no), nb_lgnd_nth_state(n, en));
            gtk_label_set_use_markup(GTK_LABEL(no), true);
            gtk_list_box_row_set_selectable(row, en);
            gtk_widget_set_opacity(GTK_WIDGET(row), en ? 1 : 0.5);
            if (pinger_state.gotdata && !pinger_state.run) graphtab_graph_refresh(false);
            LOG("graph exclusion mask: 0x%x", lgnd_excl_mask);
          }
        }
      }
    }
  }
}

#ifdef WITH_DND
static GdkContentProvider* nt_lgnd_dnd_drag(GtkDragSource *src, double x, double y, t_notifier *nt) {
  if (!nt) return NULL;
  nt->dx = round(x); nt->dy = round(y);
  DEBUG("DND-drag: x=%d y=%d, dx=%d dy=%d", nt->x, nt->y, nt->dx, nt->dy);
  return gdk_content_provider_new_typed(G_TYPE_POINTER, nt);
}

static void nt_lgnd_dnd_icon(GtkDragSource *src, GdkDrag *drag, t_notifier *nt) {
  if (!GTK_IS_DRAG_SOURCE(src) || !nt || !GTK_IS_WIDGET(nt->box)) return;
  GdkPaintable *icon = gtk_widget_paintable_new(nt->box);
  if (!GDK_IS_PAINTABLE(icon)) return;
  gtk_drag_source_set_icon(src, icon, nt->dx, nt->dy);
  g_object_unref(icon);
  DEBUG("DND-icon: dx=%d dy=%d", nt->dx, nt->dy);
}

static gboolean nt_lgnd_on_drop(GtkDropTarget *dst, const GValue *val, double x, double y, gpointer unused) {
  if (!val || !G_VALUE_HOLDS_POINTER(val)) return false;
  t_notifier *nt = g_value_get_pointer(val);
  if (!nt) return false;
  nt->x = round(x) - nt->dx;
  nt->y = round(y) - nt->dy;
  DEBUG("DND-drop: x=%d y=%d, dx=%d dy=%d, cursor at x=%.0f y=%.0f", nt->x, nt->y, nt->dx, nt->dy, x, y);
  return true;
}
#endif

#define NT_LG_LABEL(lab, txt, align, chars, visible) { lab = gtk_label_new(txt); \
  nt_add_lgelem(lab, ex->box, align, chars, visible); }

static GtkWidget* nt_init(GtkWidget *base, t_notifier *nt) {
  if (!nt || !GTK_IS_WIDGET(base)) return NULL;
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(GTK_IS_OVERLAY(over), NULL);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(box), NULL);
  if (style_loaded) {
    if (nt->defcss) gtk_widget_add_css_class(box, nt->defcss);
    if (nt->colcss) gtk_widget_add_css_class(box, nt->colcss);
    if (nt->dyncss) gtk_widget_add_css_class(box, nt_dark ? CSS_BGONDARK : CSS_BGONLIGHT);
  }
  GtkWidget *inbox = NULL;
  switch (nt->type) {
    case NT_MAIN_NDX: {
      inbox = gtk_label_new(NULL);
      if (GTK_IS_LABEL(inbox)) {
        if (style_loaded && nt->dyncss) gtk_widget_add_css_class(inbox, CSS_INVERT);
      } else FAILX("action notifier", "gtk_label_new()");
    } break;
    case NT_GRAPH_NDX: {
      inbox = gtk_list_box_new();
      if (GTK_IS_LIST_BOX(inbox)) {
        g_signal_connect(inbox, EV_ROW_ACTIVE, G_CALLBACK(nt_lgnd_row_cb), NULL);
        if (style_loaded) gtk_widget_add_css_class(inbox, CSS_GR_BG);
        for (int i = 0; i < MAXTTL; i++) {
          t_nt_extra *ex = nt->extra ? &(nt->extra[i]) : NULL;
          if (!ex) continue;
          ex->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
          if (GTK_IS_BOX(ex->box)) { // number, color dash, avrg±jttr, cc:as, hopname
            gchar span[100]; g_snprintf(span, sizeof(span), LGND_LINE_TOGGLE_TIP, i + 1);
            gtk_widget_set_tooltip_text(ex->box, span);
            ex->row = line_row_new(ex->box, false);
            if (GTK_IS_LIST_BOX_ROW(ex->row)) {
              gtk_list_box_append(GTK_LIST_BOX(inbox), GTK_WIDGET(ex->row));
              gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(ex->row), LGND_ROW_DEF_STATE);
              if (style_loaded) {
                gtk_widget_add_css_class(GTK_WIDGET(ex->row), CSS_LEGEND_TEXT);
                if (nt->colcss) gtk_widget_add_css_class(GTK_WIDGET(ex->row), nt->colcss);
              }
              NT_LG_LABEL(ex->elem[GRLG_NO], nb_lgnd_nth_state(i, LGND_ROW_DEF_STATE), GTK_ALIGN_END, 2, true);
              if (GTK_IS_WIDGET(ex->elem[GRLG_NO])) gtk_label_set_use_markup(GTK_LABEL(ex->elem[GRLG_NO]), true);
              { // colored dash
                gchar *col = get_nth_color(i);
                if (col) g_snprintf(span, sizeof(span), LGND_DASH_COL_FMT, col, DASH_CHAR);
                else g_snprintf(span, sizeof(span), LGND_DASH_DEF_FMT, DASH_CHAR);
                NT_LG_LABEL(ex->elem[GRLG_DASH], span, -1, -1, is_grelem_enabled(GRLG_DASH));
                if (GTK_IS_WIDGET(ex->elem[GRLG_DASH]) && col) gtk_label_set_use_markup(GTK_LABEL(ex->elem[GRLG_DASH]), true);
                g_free(col); }
              NT_LG_LABEL(ex->elem[GRLG_AVJT], NULL, GTK_ALIGN_START, -1, is_grelem_enabled(GRLG_AVJT));
              NT_LG_LABEL(ex->elem[GRLG_CCAS], NULL, GTK_ALIGN_START, -1, is_grelem_enabled(GRLG_CCAS));
              NT_LG_LABEL(ex->elem[GRLG_LGHN], NULL, GTK_ALIGN_START, -1, is_grelem_enabled(GRLG_LGHN));
            } else GL_FAIL("gtk_list_box_new()");
          } else GL_FAIL("gtk_box_new()");
        }
#ifdef WITH_DND
        GtkDragSource *dnd_src = gtk_drag_source_new();
        if (GTK_IS_DRAG_SOURCE(dnd_src)) {
          g_signal_connect(dnd_src, EV_DND_DRAG, G_CALLBACK(nt_lgnd_dnd_drag), nt);
          g_signal_connect(dnd_src, EV_DND_ICON, G_CALLBACK(nt_lgnd_dnd_icon), nt);
          gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(dnd_src));
        } else GL_FAIL("gtk_drag_source_new()");
        GtkDropTarget *dnd_dst = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_COPY);
        if (GTK_IS_DROP_TARGET(dnd_dst)) {
          g_signal_connect(dnd_dst, EV_DND_DROP, G_CALLBACK(nt_lgnd_on_drop), NULL);
          gtk_widget_add_controller(over, GTK_EVENT_CONTROLLER(dnd_dst));
        } else GL_FAIL("gtk_drop_target_new()");
#endif
      } else GL_FAIL("gtk_list_box_new()");
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
  if (style_loaded && (nt->type == NT_GRAPH_NDX)) gtk_widget_add_css_class(box, CSS_GR_BG);
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

static void nt_update_legend_width(int type, int ndx, int max) {
  if ((type >= 0) && (type < NT_NDX_MAX) && (ndx >= 0) && (ndx < GRLG_MAX)) {
    t_nt_extra *ex = notifier[type].extra;
    if (ex) for (int i = 0; i < MAXTTL; i++, ex++)
      if (GTK_IS_LABEL(ex->elem[ndx])) gtk_label_set_width_chars(GTK_LABEL(ex->elem[ndx]), max);
  }
}


// pub
//

inline GtkWidget* notifier_init(int ndx, GtkWidget *base) {
  return ((ndx >= 0) && (ndx < NT_NDX_MAX)) ? nt_init(base, &notifier[ndx]) : NULL;
}

void notifier_inform(const gchar *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  gchar *str = g_strdup_vprintf(fmt, ap);
  if (str) { nt_inform(&notifier[NT_MAIN_NDX], str); g_free(str); }
  va_end(ap);
}

inline gboolean notifier_get_visible(int ndx) {
  return ((ndx >= 0) && (ndx < NT_NDX_MAX)) ? notifier[ndx].visible : true;
}

inline void notifier_set_visible(int ndx, gboolean visible) {
  if ((ndx >= 0) && (ndx < NT_NDX_MAX)) nt_set_visible(&notifier[ndx], visible);
}

#define NT_LG_ELEM_VIS(ndx, extra_cond) { if (GTK_IS_WIDGET(ex->elem[ndx])) \
  gtk_widget_set_visible(ex->elem[ndx], vis && is_grelem_enabled(ndx) && extra_cond); }

void notifier_legend_vis_rows(int max) {
  static int nt_lgnd_max;
  if (max < 0) max = nt_lgnd_max; else nt_lgnd_max = max;
  t_nt_extra *ex = notifier[NT_GRAPH_NDX].extra;
  if (ex) for (int i = 0; i < MAXTTL; i++, ex++) {
    gboolean vis = (i >= opts.min) && (i < max);
    if (GTK_IS_WIDGET(ex->row)) gtk_widget_set_visible(GTK_WIDGET(ex->row), vis);
    if (GTK_IS_WIDGET(ex->box)) gtk_widget_set_visible(ex->box, vis);
    NT_LG_ELEM_VIS(GRLG_DASH, true);
    NT_LG_ELEM_VIS(GRLG_AVJT, true);
    NT_LG_ELEM_VIS(GRLG_CCAS, opts.whois);
    NT_LG_ELEM_VIS(GRLG_LGHN, true);
  }
  gboolean vis = notifier_get_visible(NT_GRAPH_NDX);
  if (opts.legend && !vis) notifier_set_visible(NT_GRAPH_NDX, true);
  if (!opts.legend && vis) notifier_set_visible(NT_GRAPH_NDX, false);
}

#define LGFL_CHK_MAX(ndx, max) { if (max != nt_lgfl_max[ndx]) { \
  nt_lgfl_max[ndx] = max; nt_update_legend_width(NT_GRAPH_NDX, ndx, max); }}

void notifier_legend_update(void) {
  static int nt_lgfl_max[GRLG_MAX];
  t_nt_extra *ex = notifier[NT_GRAPH_NDX].extra;
  if (!ex) return;
  int aj_max = -1, ca_max = -1;
  for (int i = 0; i < hops_no; i++, ex++) {
    t_legend legend; stat_legend(i, &legend);
    if (legend.name && GTK_IS_LABEL(ex->elem[GRLG_LGHN])) UPDATE_LABEL(ex->elem[GRLG_LGHN], legend.name);
    nt_fill_legend_elem(ex->elem[GRLG_AVJT], legend.jt, legend.av, "±", &aj_max);
    nt_fill_legend_elem(ex->elem[GRLG_CCAS], legend.as, legend.cc, ":", &ca_max);
  }
  LGFL_CHK_MAX(GRLG_AVJT, aj_max);
  LGFL_CHK_MAX(GRLG_CCAS, ca_max);
}

void notifier_legend_reload_css(void) {
  t_notifier *nt = &notifier[NT_GRAPH_NDX];
  if (!nt && !style_loaded) return;
  if (nt->box) { gtk_widget_remove_css_class(nt->box, CSS_GR_BG); gtk_widget_add_css_class(nt->box, CSS_GR_BG); }
  if (nt->inbox) { gtk_widget_remove_css_class(nt->inbox, CSS_GR_BG); gtk_widget_add_css_class(nt->inbox, CSS_GR_BG); }
}

unsigned lgnd_excl_mask; // enough for MAXTTL(30) bitmask
gboolean nt_dark, nt_on_graph;

