
#include "notifier.h"
#include "style.h"
#include "pinger.h"
#include "stat.h"
#include "tabs/graph.h"
#include "ui/action.h"

#define DASH_CHAR "—"
#define LGND_ROW_DEF_STATE true
#define LGND_DIS_MARK_FMT "<span strikethrough='true' strikethrough_color='#ff0000'>%d</span>"
#define LGND_DASH_DEF_FMT "<span weight='800'>%s</span>"
#define LGND_DASH_COL_FMT "<span weight='800' color='%s'>%s</span>"
#define LGND_LINE_TOGGLE_TIP "show/hide graph #%d"

typedef struct nt_leg {
  GtkListBoxRow *row;      // row
  GtkWidget *box;          // box in row
  GtkWidget *elem[GE_MAX]; // dash, cc:as, avrg±jttr, cc:as, hopname
} t_nt_leg;

typedef struct ntcss { const char *def, *col; } t_ntcss;

typedef struct notifier {
  GtkWidget *box, *inbox, *reveal; t_ntcss css;
  void* const content; int content_n;
  const int type;
  const gboolean autohide, dyncss;
  gboolean visible;
  int x, y;
#ifdef WITH_DND
  int dx, dy;
#endif
#ifdef WITH_PLOT
  gboolean onright;
#endif
} t_notifier;

#ifdef WITH_PLOT
typedef struct rotor_arrow { const char *alt; const char *ico[MAX_ICONS]; } t_rotor_arrow;
#endif

unsigned lgnd_excl_mask; // enough for MAXTTL(30) bitmask
gboolean nt_dark;

//

static t_nt_leg nt_leg[MAXTTL];

static t_notifier notifier[] = {
  { .type = NT_MAIN_NDX, .autohide = true, .dyncss = true, .css.def = CSS_ROUNDED },
  { .type = NT_LEGEND_NDX, .css = { .def = CSS_LEGEND, .col = CSS_LEGEND_COL },
    .x = GRAPH_LEFT + MARGIN * 2, .y = GRAPH_TOP + MARGIN * 2,
    .content = nt_leg, .content_n = G_N_ELEMENTS(nt_leg) },
#ifdef WITH_PLOT
  { .type = NT_ROTOR_NDX, .css = { .def = CSS_ROTOR, .col = CSS_PLOT_BG },
    .x = 50, .y = 50, .onright = true },
#endif
};
#define NT_VALID_NDX(ndx) ((ndx >= 0) && (ndx < G_N_ELEMENTS(notifier)))

//

static void nt_reload_css(GtkWidget *w, const char *css) {
  if (w) {
    if (gtk_widget_has_css_class(w, css)) gtk_widget_remove_css_class(w, css);
    gtk_widget_add_css_class(w, css); }
}

static gboolean nt_set_visible(t_notifier *nt, gboolean visible) {
  if (!atquit && nt && (nt->visible != visible)) {
    if (GTK_IS_REVEALER(nt->reveal)) gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), visible);
    nt->visible = visible;
  }
  return G_SOURCE_REMOVE;
}

static gboolean nt_hide(t_notifier *nt) { return nt_set_visible(nt, false); }

static void nt_inform(t_notifier *nt, char *mesg) {
  static int nt_curr_dark;
  if (!nt || !mesg) return;
  if (!GTK_IS_WIDGET(nt->inbox) || !GTK_IS_REVEALER(nt->reveal)) return;
  if (nt->autohide && nt->visible) nt_hide(nt);
  if (GTK_IS_LABEL(nt->inbox)) {
    LOG("%s", mesg);
    gtk_label_set_text(GTK_LABEL(nt->inbox), mesg);
  }
  if (nt->dyncss && (nt_dark != nt_curr_dark)) {
    gtk_widget_remove_css_class(nt->box, nt_curr_dark ? CSS_BGONDARK : CSS_BGONLIGHT);
    nt_curr_dark = nt_dark;
    gtk_widget_add_css_class(nt->box,    nt_curr_dark ? CSS_BGONDARK : CSS_BGONLIGHT);
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(nt->reveal), true);
  if (nt->autohide) nt->visible = g_timeout_add_seconds(AUTOHIDE_IN, (GSourceFunc)nt_hide, nt);
}

static gboolean nt_ext_pos_cb(GtkOverlay *unused, GtkWidget *widget, GdkRectangle *r, t_notifier *nt) {
  gboolean re = r && nt && GTK_IS_WIDGET(widget);
  if (re) {
    int w = 0;
    gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w,   NULL, NULL);
    gtk_widget_measure(widget, GTK_ORIENTATION_VERTICAL,   -1, NULL, NULL, NULL, NULL);
#ifdef WITH_PLOT
    if (nt->onright/*once*/ && w) { nt->x = r->width - w - nt->x; nt->onright = false; }
#endif
    r->x = nt->x; r->y = nt->y;
  }
  return re;
}

static void nt_add_lgelem(GtkWidget *widget, GtkWidget *box, int align, gboolean visible) {
  if (!GTK_IS_BOX(box) || !GTK_IS_WIDGET(widget)) return;
  gtk_widget_set_visible(widget, visible);
  gtk_box_append(GTK_BOX(box), widget);
  if (align >= 0) {
    gtk_widget_set_halign(widget, align);
    if (GTK_IS_LABEL(widget)) gtk_label_set_xalign(GTK_LABEL(widget), align == GTK_ALIGN_END);
  }
  if (style_loaded) nt_reload_css(widget, CSS_LEGEND_TEXT);
}

static void nt_reveal_sets(GtkWidget *reveal, int align, int transition) {
  if (!GTK_IS_REVEALER(reveal)) return;
  gtk_widget_set_halign(reveal, align);
  gtk_widget_set_valign(reveal, align);
  gtk_revealer_set_transition_type(GTK_REVEALER(reveal), transition);
}

static char* nb_lgnd_nth_state(int n, gboolean enable) {
  static char nb_lgnd_state_buff[80]; n++;
  if (enable) g_snprintf(nb_lgnd_state_buff, sizeof(nb_lgnd_state_buff), "%d", n);
  else g_snprintf(nb_lgnd_state_buff, sizeof(nb_lgnd_state_buff), LGND_DIS_MARK_FMT, n);
  return nb_lgnd_state_buff;
}

static void nt_lgnd_row_cb(GtkListBox* self, GtkListBoxRow* row, void *data) {
  if (GTK_IS_LIST_BOX_ROW(row)) {
    GtkWidget *box = gtk_list_box_row_get_child(row);
    if (GTK_IS_BOX(box)) {
      GtkWidget *no = gtk_widget_get_first_child(box);
      if (GTK_IS_LABEL(no)) {
        const char *txt = gtk_label_get_text(GTK_LABEL(no));
        if (txt) {
          int n = atoi(txt); n--;
          if ((n >= opts.min) && (n < opts.lim)) {
            gboolean en = !gtk_list_box_row_get_selectable(row);
            if (en) lgnd_excl_mask &= ~(en << n); else lgnd_excl_mask |= (!en << n);
            gtk_label_set_text(GTK_LABEL(no), nb_lgnd_nth_state(n, en));
            gtk_label_set_use_markup(GTK_LABEL(no), true);
            gtk_list_box_row_set_selectable(row, en);
            gtk_widget_set_opacity(GTK_WIDGET(row), en ? 1 : 0.5);
            if (pinger_state.gotdata && !pinger_state.run) graphtab_update();
            LOG("graph exclusion mask: 0x%x", lgnd_excl_mask);
          }
        }
      }
    }
  }
}

#ifdef WITH_DND
static GdkContentProvider* nt_dnd_drag(GtkDragSource *unused, gdouble x, gdouble y, t_notifier *nt) {
  GdkContentProvider *prov = NULL;
  if (nt) {
    nt->dx = round(x); nt->dy = round(y);
    DNDORD("DND-drag: x=%d y=%d, dx=%d dy=%d", nt->x, nt->y, nt->dx, nt->dy);
    prov = gdk_content_provider_new_typed(G_TYPE_POINTER, nt); }
  return prov;
}

static void nt_dnd_icon(GtkDragSource *src, GdkDrag *drag, t_notifier *nt) {
  if (GTK_IS_DRAG_SOURCE(src) && nt && GTK_IS_WIDGET(nt->box)) {
    GdkPaintable *icon = gtk_widget_paintable_new(nt->box);
    if (icon) {
      gtk_drag_source_set_icon(src, icon, nt->dx, nt->dy);
      g_object_unref(icon);
      DNDORD("DND-icon: dx=%d dy=%d", nt->dx, nt->dy); }}
}

static gboolean nt_on_drop(GtkDropTarget *unused, const GValue *val, gdouble x, gdouble y, t_notifier *receiver) {
  gboolean re = receiver && val && G_VALUE_HOLDS_POINTER(val);
  if (re) {
    t_notifier *sender = g_value_get_pointer(val);
    re = sender && (sender == receiver);
    if (re) {
      int sx = round(x) - sender->dx, sy = round(y) - sender->dy;
      DNDORD("DND-drop: x=%d y=%d, dx=%d dy=%d, cursor at x=%.0f y=%.0f", sx, sy, sender->dx, sender->dy, x, y);
      LOG("%s relocation: (%d %d) => (%d %d)", sender->css.def, sender->x, sender->y, sx, sy);
      sender->x = sx; sender->y = sy; }}
  return re;
}
#endif

static inline void nt_make_leg_row(GtkListBox *box, GtkSizeGroup* group[], int i, t_nt_leg *leg, const char *css) {
#define LEG_GROUP(n, setuse) { if (group[n]) gtk_size_group_add_widget(group[n], leg->elem[n]); \
  gtk_label_set_use_markup(GTK_LABEL(leg->elem[n]), setuse); }
#define LEG_LABEL(lab, txt, align, vis) { lab = gtk_label_new(txt); nt_add_lgelem(lab, leg->box, align, vis); }
  if (!box || !leg) return;
  gtk_list_box_append(box, GTK_WIDGET(leg->row));
  gtk_list_box_row_set_selectable(leg->row, LGND_ROW_DEF_STATE);
  if (style_loaded) {
    nt_reload_css(GTK_WIDGET(leg->row), CSS_LEGEND_TEXT);
    if (css) nt_reload_css(GTK_WIDGET(leg->row), css);
  }
  LEG_LABEL(leg->elem[GE_NO], nb_lgnd_nth_state(i, LGND_ROW_DEF_STATE), GTK_ALIGN_END, true);
  if (GTK_IS_WIDGET(leg->elem[GE_NO])) LEG_GROUP(GE_NO, true);
  { // colored dash
    char span[100], *col = get_nth_color(i);
    if (col) g_snprintf(span, sizeof(span), LGND_DASH_COL_FMT, col, DASH_CHAR);
    else g_snprintf(span, sizeof(span), LGND_DASH_DEF_FMT, DASH_CHAR);
    LEG_LABEL(leg->elem[GE_DASH], span, -1, is_grelem_enabled(GE_DASH));
    if (GTK_IS_WIDGET(leg->elem[GE_DASH]) && col) LEG_GROUP(GE_DASH, true);
    g_free(col);
  }
  for (int j = GE_MIN; j < GE_MAX; j++) {
    int ndx = graphelem_type2ndx(j);
    if (IS_GRLG_NDX(ndx)) { leg->elem[ndx] = gtk_label_new(NULL); LEG_GROUP(ndx, false); }
  }
  for (int j = GE_MIN; j < GE_MAX; j++)
    nt_add_lgelem(leg->elem[j], leg->box, GTK_ALIGN_START, is_grelem_enabled(graphelem[j].type));
#undef LEG_LABEL
#undef LEG_GROUP
}

#ifdef WITH_DND
static void nt_init_dnd_over(GtkWidget *inbox, GtkWidget *over, t_notifier *nt) {
  if (!GTK_IS_WIDGET(inbox) || !GTK_IS_OVERLAY(over) || !nt) return;
  GtkDragSource *src = gtk_drag_source_new();
  if (src) {
    g_signal_connect(src, EV_DND_DRAG, G_CALLBACK(nt_dnd_drag), nt);
    g_signal_connect(src, EV_DND_ICON, G_CALLBACK(nt_dnd_icon), nt);
    gtk_widget_add_controller(inbox, GTK_EVENT_CONTROLLER(src));
  }
  GtkDropTarget *dst = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_COPY);
  if (dst) {
    g_signal_connect(dst, EV_DND_DROP, G_CALLBACK(nt_on_drop), nt);
    gtk_widget_add_controller(over, GTK_EVENT_CONTROLLER(dst));
  }
}
#endif

static void nt_init_legend(GtkWidget *inbox, GtkWidget *over, t_notifier *nt) {
  if (!GTK_IS_LIST_BOX(inbox) || !nt) return;
  g_signal_connect(inbox, EV_ROW_ACTIVE, G_CALLBACK(nt_lgnd_row_cb), NULL);
  if (style_loaded) nt_reload_css(inbox, CSS_GRAPH_BG);
  GtkSizeGroup* group[GE_MAX];
  for (int i = 0; i < G_N_ELEMENTS(group); i++)
    group[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  t_nt_leg *leg = nt->content ? nt->content : NULL; int n = nt->content_n;
  for (int i = 0; leg && (i < n); i++, leg++) {
    leg->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
    if (leg->box) { // number, color dash, avrg±jttr, cc:as, hopname
      char span[100]; g_snprintf(span, sizeof(span), LGND_LINE_TOGGLE_TIP, i + 1);
      gtk_widget_set_tooltip_text(leg->box, span);
      leg->row = line_row_new(leg->box, false);
      if (leg->row) nt_make_leg_row(GTK_LIST_BOX(inbox), group, i, leg, nt->css.col);
    }
  }
  for (int i = 0; i < G_N_ELEMENTS(group); i++) if (group[i]) g_object_unref(group[i]);
#ifdef WITH_DND
  nt_init_dnd_over(inbox, over, nt);
#endif
}

#ifdef WITH_PLOT
static void nt_rotor_cb(GtkButton *unused, t_kb_plot_aux *kb) { if (kb) on_rotation(NULL, NULL, kb); }

static void nt_init_rotor(GtkWidget *inbox, GtkWidget *over, t_notifier *nt) {
  if (!GTK_IS_BOX(inbox)) return;
  t_rotor_arrow rc01 = {.alt = "UP",    .ico = {RTR_UP_ICON,    RTR_UP_ICOA,    RTR_UP_ICOB   }};
  t_rotor_arrow rc10 = {.alt = "LEFT",  .ico = {RTR_LEFT_ICON,  RTR_LEFT_ICOA,  RTR_LEFT_ICOB }};
  t_rotor_arrow rc11 = {.alt = "ROLL",  .ico = {RTR_ROLL_ICON,  RTR_ROLL_ICOA,  RTR_ROLL_ICOB }};
  t_rotor_arrow rc12 = {.alt = "RIGHT", .ico = {RTR_RIGHT_ICON, RTR_RIGHT_ICOA, RTR_RIGHT_ICOB}};
  t_rotor_arrow rc21 = {.alt = "DOWN",  .ico = {RTR_DOWN_ICON,  RTR_DOWN_ICOA,  RTR_DOWN_ICOB }};
  t_rotor_arrow* rotor_cntrl[3][3] = {
    {NULL,  &rc01, NULL },
    {&rc10, &rc11, &rc12},
    {NULL,  &rc21, NULL },
  };
  t_kb_plot_aux* kb_cntrl[3][3] = {
    {&kb_plot_aux[ACCL_SA_PGDN], &kb_plot_aux[ACCL_SA_UP],   &kb_plot_aux[ACCL_SA_PGUP] },
    {&kb_plot_aux[ACCL_SA_LEFT], &kb_plot_aux[ACCL_SA_PGUP], &kb_plot_aux[ACCL_SA_RIGHT]},
    {&kb_plot_aux[ACCL_SA_PGDN], &kb_plot_aux[ACCL_SA_DOWN], &kb_plot_aux[ACCL_SA_PGUP] },
  };
  GtkSizeGroup* group[G_N_ELEMENTS(rotor_cntrl[0])];
  for (int i = 0; i < G_N_ELEMENTS(group); i++)
    group[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  for (int i = 0; i < G_N_ELEMENTS(rotor_cntrl); i++) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (row) {
      gtk_box_append(GTK_BOX(inbox), row);
      for (int j = 0; j < G_N_ELEMENTS(rotor_cntrl[j]); j++) {
        t_rotor_arrow *a = rotor_cntrl[i][j]; GtkWidget *elem = NULL;
        if (a) {
          const char *ico = is_sysicon(a->ico);
          elem = ico ? gtk_button_new_from_icon_name(ico) : gtk_button_new_with_label(a->alt);
        } else elem = gtk_button_new_with_label(NULL);
        if (!elem) continue;
        gtk_button_set_has_frame(GTK_BUTTON(elem), false);
        gtk_button_set_use_underline(GTK_BUTTON(elem), false);
        gtk_widget_add_css_class(elem, CSS_ROTOR_COL);
        if (group[j]) gtk_size_group_add_widget(group[j], elem);
        g_signal_connect(elem, EV_CLICK, G_CALLBACK(nt_rotor_cb), kb_cntrl[i][j]);
        gtk_box_append(GTK_BOX(row), elem);
      }
    }
  }
  for (int i = 0; i < G_N_ELEMENTS(group); i++) if (group[i]) g_object_unref(group[i]);
#ifdef WITH_DND
  nt_init_dnd_over(inbox, over, nt);
#endif
}
#endif

static GtkWidget* nt_init(GtkWidget *base, t_notifier *nt) {
  if (!GTK_IS_WIDGET(base) || !nt) return NULL;
  GtkWidget *over = gtk_overlay_new();                        g_return_val_if_fail(over, NULL);
  GtkWidget *box  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); g_return_val_if_fail(box, NULL);
  GtkWidget *rev  = gtk_revealer_new();                       g_return_val_if_fail(rev, NULL);
  gtk_revealer_set_reveal_child(GTK_REVEALER(rev), false);
  nt->box = box; nt->reveal = rev;
  if (style_loaded) {
    if (nt->css.def) gtk_widget_add_css_class(box, nt->css.def);
    if (nt->css.col) gtk_widget_add_css_class(box, nt->css.col);
    if (nt->dyncss) gtk_widget_add_css_class(box, nt_dark ? CSS_BGONDARK : CSS_BGONLIGHT);
  }
  GtkWidget *inbox = NULL;
  switch (nt->type) {
    case NT_MAIN_NDX: {
      inbox = gtk_label_new(NULL);
      if (inbox && style_loaded && nt->dyncss) gtk_widget_add_css_class(inbox, CSS_INVERT);
    } break;
    case NT_LEGEND_NDX: {
      inbox = gtk_list_box_new();
      nt_init_legend(inbox, over, nt);
    } break;
#ifdef WITH_PLOT
    case NT_ROTOR_NDX: {
      inbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      if (inbox) {
        nt_init_rotor(inbox, over, nt);
        nt->visible = is_plelem_enabled(D3_ROTR);
        gtk_revealer_set_reveal_child(GTK_REVEALER(rev), nt->visible);
      }
    } break;
#endif
    default: WARN("Unknown notifier type: %d\n", nt->type);
  }
  g_return_val_if_fail(inbox, NULL);
  nt->inbox = inbox;
  gtk_widget_set_visible(inbox, true);
  gtk_widget_set_can_focus(inbox, false);
  gtk_widget_set_hexpand(inbox, true);
  gtk_widget_set_halign(inbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(inbox, GTK_ALIGN_CENTER);
  gtk_widget_set_visible(rev, true);
  gtk_widget_set_can_focus(rev, false);
  gtk_widget_set_hexpand(rev, true);
  if ((nt->x > 0) || (nt->y > 0)) {
    nt_reveal_sets(rev, GTK_ALIGN_START, GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    g_signal_connect(over, EV_GET_POS, G_CALLBACK(nt_ext_pos_cb), nt);
  } else nt_reveal_sets(rev, GTK_ALIGN_CENTER, GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT);
  // link widgets together
  gtk_box_append(GTK_BOX(box), inbox);
  if (style_loaded && (nt->type == NT_LEGEND_NDX)) gtk_widget_add_css_class(box, CSS_GRAPH_BG);
  gtk_revealer_set_child(GTK_REVEALER(rev), box);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), rev);
  gtk_overlay_set_child(GTK_OVERLAY(over), base);
  return over;
}

static void nt_fill_legend_elem(GtkWidget* label, const char *f1, const char *f2, const char *delim) {
  if (!GTK_IS_LABEL(label)) return;
  char *str =((f1) && (f1)[0])
    ? g_strdup_printf("%s%s%s", (f2) ? (f2) : "", delim ? delim : "", f1)
    : ((f2) ? g_strdup_printf("%s", f2) : NULL);
  UPDATE_LABEL(label, str);
  g_free(str);
}


// pub
//

inline GtkWidget* notifier_init(int ndx, GtkWidget *base) { return NT_VALID_NDX(ndx) ? nt_init(base, &notifier[ndx]) : NULL; }

void notifier_inform(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *str = g_strdup_vprintf(fmt, ap);
  if (str) { nt_inform(&notifier[NT_MAIN_NDX], str); g_free(str); }
  va_end(ap);
}

inline gboolean notifier_get_visible(int ndx) { return NT_VALID_NDX(ndx) ? notifier[ndx].visible : false; }
inline void notifier_set_visible(int ndx, gboolean visible) { if (NT_VALID_NDX(ndx)) nt_set_visible(&notifier[ndx], visible); }

void notifier_legend_vis_rows(int upto) {
  static int nt_lgnd_vis_rows;
  if (upto < 0) upto = nt_lgnd_vis_rows; else nt_lgnd_vis_rows = upto;
  t_nt_leg *leg = notifier[NT_LEGEND_NDX].content;
  if (leg) for (int i = 0; i < MAXTTL; i++, leg++) {
    gboolean vis = (i >= opts.min) && (i < upto);
    if (GTK_IS_WIDGET(leg->row)) gtk_widget_set_visible(GTK_WIDGET(leg->row), vis);
    if (GTK_IS_WIDGET(leg->box)) gtk_widget_set_visible(leg->box, vis);
    for (int j = GE_MIN; j < GE_MAX; j++) if (GTK_IS_WIDGET(leg->elem[j]))
      gtk_widget_set_visible(leg->elem[j], vis && graphelem[j].enable &&
        ((graphelem[j].type == GE_CCAS) ? opts.whois : true));
  }
  gboolean vis = notifier_get_visible(NT_LEGEND_NDX);
  if (opts.legend && !vis) notifier_set_visible(NT_LEGEND_NDX, true);
  if (!opts.legend && vis) notifier_set_visible(NT_LEGEND_NDX, false);
}

void notifier_legend_update(void) {
  t_nt_leg *leg = notifier[NT_LEGEND_NDX].content; int n = notifier[NT_LEGEND_NDX].content_n;
  for (int i = 0; leg && (i < n) && (i < tgtat); i++, leg++) {
    t_legend legend; stat_legend(i, &legend);
    for (int j = GE_MIN; j < GE_MAX; j++) {
      GtkWidget *w = leg->elem[j];
      switch (graphelem[j].type) {
        case GE_AVJT: nt_fill_legend_elem(w, legend.jt, legend.av, "±"); break;
        case GE_CCAS: nt_fill_legend_elem(w, legend.as, legend.cc, ":"); break;
        case GE_LGHN: if (legend.name && GTK_IS_LABEL(w)) UPDATE_LABEL(w, legend.name); break;
      }
    }
  }
}

inline void notifier_legend_refresh(void) {
  if (style_loaded) { // css dependent redrawing
    t_notifier *nt = &notifier[NT_LEGEND_NDX];
    if (nt->box) {
      if (nt->css.def) nt_reload_css(nt->box, nt->css.def);
      if (nt->css.col) nt_reload_css(nt->box, nt->css.col);
    }
    if (nt->inbox) nt_reload_css(nt->inbox, CSS_GRAPH_BG);
  }
}

