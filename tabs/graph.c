
#include "graph.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/notifier.h"

#define X_AXIS "time"
#define Y_AXIS "delay [msec]"

#define GR_FONT "monospace"
#define GRF_RATIO 0.28

#define GR_RTT0 1000 // 1msec
#define GR_RTT_SCALE 1000. // usec to msec
#define GR_Y_GAP 1.1

#define TICK_SIZE 6
#define DASH_BOLD 0.7
#define DOT_SIZE  5
#define LINE_SIZE 2

#define X_CELL_SEC 5
#define X_FREQ     2

typedef struct gr_aux_measures {
  int x, y, x1, y1, w, h, N, M, fs, no, ymax, at;
  double dx;
} t_gr_aux_measures;

typedef struct gr_list {
  GSList* list; int len; // to not calculate length of list
} t_gr_list;

typedef struct gr_point_desc {
  double x;
  int rtt;
  gboolean conn;
} t_gr_point_desc;

typedef void (*draw_smth_fn)(int i, cairo_t *cr, double x);

static t_tab graphtab = { .self = &graphtab, .name = "graph-tab",
  .tag = GRAPH_TAB_TAG, .tip = GRAPH_TAB_TIP, .ico = {GRAPH_TAB_ICON, GRAPH_TAB_ICOA, GRAPH_TAB_ICOB},
//  .desc = { [POP_MENU_NDX_COPY] = { .name = "win.graph_menu_copy" }, [POP_MENU_NDX_SALL] = { .name = "win.graph_menu_sall" }},
//  .act = { [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l1 },  [POP_MENU_NDX_SALL] = { .activate = cb_on_sall }},
};

static t_gr_list gr_series[MAXTTL];
#define GR_LIST (gr_series[i].list)
#define GR_LEN (gr_series[i].len)
static const int n_series = G_N_ELEMENTS(gr_series);

static GtkWidget *graph_grid, *graph_marks, *graph_graph;
static PangoFontDescription *graph_font;

static t_gr_aux_measures grm = { .x = GRAPH_LEFT, .y = GRAPH_TOP, .fs = CELL_SIZE * GRF_RATIO, .ymax = GR_RTT0,
  .dx = (MAIN_TIMING_MSEC / 1000.) * ((double)CELL_SIZE / X_CELL_SEC),
  .no = X_RES / ((double)CELL_SIZE / X_CELL_SEC) + 1, // to start keeping data w/o knowing drawing area width
};

//

static void gr_scale_max(int max) {
  grm.ymax = max * GR_Y_GAP;
  gtk_widget_queue_draw(graph_marks);
  gtk_widget_set_visible(graph_marks, true);
}

static int gr_max_in_series(void) {
  int max = GR_RTT0;
  for (int i = 0; i < n_series; i++) {
    for (GSList *item = GR_LIST; item; item = item->next) {
      if (!item->data) continue;
      int rtt = ((t_stat_graph*)item->data)->rtt;
      if (rtt > max) max = rtt;
    }
  }
  return max;
}

static void gr_save(int i, t_stat_graph data) {
  t_stat_graph *sd = g_memdup2(&data, sizeof(data));
  GR_LIST = g_slist_prepend(GR_LIST, sd); GR_LEN++;
  if (sd && (sd->rtt > grm.ymax)) gr_scale_max(sd->rtt); // up-scale
  while (GR_LIST && (GR_LEN > grm.no)) {
    GSList *last = g_slist_last(GR_LIST);
    if (!last) break;
    int rtt = last->data ? ((t_stat_graph*)last->data)->rtt : -1;
    GR_LIST = g_slist_delete_link(GR_LIST, last); GR_LEN--;
    if ((rtt * GR_Y_GAP) >= grm.ymax) {
      int max = gr_max_in_series();
      if (((max * GR_Y_GAP) < grm.ymax) && (max > GR_RTT0)) gr_scale_max(max); // down-scale
    }
  }
}

static void gr_set_font(void) {
  graph_font = pango_font_description_new();
  if (graph_font) {
    pango_font_description_set_family(graph_font, GR_FONT);
    int fs = grm.fs ? grm.fs : (CELL_SIZE * GRF_RATIO);
    pango_font_description_set_absolute_size(graph_font, fs * PANGO_SCALE);
  } else WARN_("Cannot allocate pango font");
}

static inline double rtt2y(double rtt) { return grm.y1 - rtt / grm.ymax * grm.h; }

static inline void gr_draw_dot_at(cairo_t *cr, double x, double rtt) {
  cairo_move_to(cr, x, rtt2y(rtt));
  cairo_close_path(cr);
}

static inline void gr_draw_line_at(cairo_t *cr, double x0, double rtt0, double x1, double rtt1) {
  cairo_move_to(cr, x0, rtt2y(rtt0));
  cairo_line_to(cr, x1, rtt2y(rtt1));
}

#define IS_RTT_DATA(d) ((d) && ((d)->rtt > 0))
#define RTT_OR_NEG(d) (IS_RTT_DATA(d) ? (d)->rtt : -1)

static inline void gr_draw_dot_loop(int i, cairo_t *cr, double x) {
  for (GSList *item = GR_LIST; item; item = item->next, x -= grm.dx) {
    if (x < grm.x) break;
    t_stat_graph *data = item->data;
    if (IS_RTT_DATA(data)) gr_draw_dot_at(cr, x, data->rtt);
  }
}

static void gr_draw_line_loop(int i, cairo_t *cr, double x) {
  t_gr_point_desc prev = { .rtt = -1, .conn = true };
  for (GSList *item = GR_LIST; item; item = item->next, x -= grm.dx) {
    if (x < grm.x) break;
    t_stat_graph *data = item->data;
    if (data) {
      gboolean connected = false;
      if (prev.rtt > 0) {
        if (data->rtt > 0) {
          gr_draw_line_at(cr, prev.x, prev.rtt, x, data->rtt);
          connected = true;
        }
      } else if (!prev.conn) gr_draw_dot_at(cr, prev.x, prev.rtt);
      prev.x = x; prev.rtt = data->rtt; prev.conn = connected;
    }
  }
}

static inline double distance(int x0, int y0, int x1, int y1) {
  int dx1 = x1 - x0; int dy1 = y1 - y0;
  return sqrt(dx1 * dx1 + dy1 * dy1);
}

static inline int centripetal(double d1, double q1, double d2, double q2, int p0, int p1, int p2) {
  return (d1 * p0 - d2 * p1 + (2 * d1 + 3 * q1 * q2 + d2) * p2) / (3 * q1 * (q1 + q2)) + 0.5;
}

static inline void gr_draw_curve_prolog(int i, cairo_t *cr, double x0) {
  if (GR_LIST) {
    t_stat_graph *data = GR_LIST->data; int r0 = RTT_OR_NEG(data);
    if (r0 > 0) {
      data = g_slist_nth_data(GR_LIST, 1); int r1 = RTT_OR_NEG(data);
      if (r1 > 0) gr_draw_line_at(cr, x0, r0, x0 - grm.dx, r1);
      else gr_draw_dot_at(cr, x0, r0);
    }
  }
}

static void gr_draw_curve_loop(int i, cairo_t *cr, double x0) {
  gr_draw_curve_prolog(i, cr, x0);
  for (GSList *item = GR_LIST; item; item = item->next, x0 -= grm.dx) {
    int r0, r1, r2, r3;
    double x1, x2, x3;
    t_stat_graph *data = item->data;  r0 = RTT_OR_NEG(data);
    data = g_slist_nth_data(item, 1); r1 = RTT_OR_NEG(data); x1 = x0 - grm.dx;
    data = g_slist_nth_data(item, 2); r2 = RTT_OR_NEG(data); x2 = x1 - grm.dx;
    data = g_slist_nth_data(item, 3); r3 = RTT_OR_NEG(data); x3 = x2 - grm.dx;
    if (x2 < grm.x) break;
    if ((r0 > 0) && (r1 > 0) && (r2 > 0) && (r3 > 0)) {
      double y0 = rtt2y(r0), y1 = rtt2y(r1), y2 = rtt2y(r2), y3 = rtt2y(r3);
      double d1 = distance(x0, y0, x1, y1);
      double d2 = distance(x1, y1, x2, y2);
      double d3 = distance(x2, y2, x3, y3);
      if ((d1 != 0) && (d2 != 0) && (d3 != 0)) { // Centripetal Catmull-Rom spline
        double q1 = sqrt(d1);
        double q2 = sqrt(d2);
        double q3 = sqrt(d3);
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr,
          centripetal(d1, q1, d2, q2, x2, x0, x1),
          centripetal(d1, q1, d2, q2, y2, y0, y1),
          centripetal(d3, q3, d2, q2, x1, x3, x2),
          centripetal(d3, q3, d2, q2, y1, y3, y2),
          x2, y2);
      } else { // Uniform Catmull-Rom spline
        cairo_move_to(cr, x1, y1);
        cairo_curve_to(cr,
          x1 + (x2 - x0) / 6,
          y1 + (y2 - y0) / 6,
          x2 - (x3 - x1) / 6,
          y2 - (y3 - y1) / 6,
          x2, y2);
      }
    } else if ((r1 > 0) && (r2 > 0)) gr_draw_line_at(cr, x1, r1, x2, r2);
    else if ((r1 > 0) && !(r0 > 0)) gr_draw_dot_at(cr, x1, r1);
  }
}

static void gr_draw_smth(cairo_t *cr, int cap, int width, draw_smth_fn draw) {
  if (!cr || !draw) return;
  if (cap >= 0) cairo_set_line_cap(cr, cap);
  if (width >= 0) cairo_set_line_width(cr, width);
  for (int i = 0; i < n_series; i++) {
    int n = i % n_colors;
    cairo_set_source_rgb(cr, colors[n][0], colors[n][1], colors[n][2]);
    draw(i, cr, grm.x1);
    cairo_stroke(cr);
  }
}

static void gr_draw_grid(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static PangoLayout *grid_pango;
  static const double dash_size[] = {1};
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !w || !h) return;
  int w0 = w - (GRAPH_LEFT + GRAPH_RIGHT), h0 = h - (GRAPH_TOP + GRAPH_BOTTOM);
  grm.N = w0 / CELL_SIZE, grm.M = h0 / CELL_SIZE;
  grm.w = grm.N * CELL_SIZE, grm.h = grm.M * CELL_SIZE;
  grm.no = grm.w / ((double)CELL_SIZE / X_CELL_SEC) + 1; // number elems per list
  grm.x1 = grm.x + grm.w; grm.y1 = grm.y + grm.h;
  // grid lines
  cairo_set_dash(cr, dash_size, 1, 0);
  cairo_set_source_rgb(cr, DASH_BOLD, DASH_BOLD, DASH_BOLD);
  for (int i = 0, x = grm.x + CELL_SIZE; i < grm.N; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, grm.y);
    cairo_rel_line_to(cr, 0, grm.h);
  }
  for (int j = 0, y = grm.y; j < grm.M; j++, y += CELL_SIZE) {
    cairo_move_to(cr, grm.x, y);
    cairo_rel_line_to(cr, grm.w, 0);
  }
  cairo_stroke(cr);
  // main axes
  cairo_set_dash(cr, dash_size, 0, 0);
  cairo_set_source_rgb(cr, 0, 0, 0);
  int ts = TICK_SIZE + 1;
  cairo_move_to(cr, grm.x, grm.y - ts);
  cairo_rel_line_to(cr, 0, grm.h + ts);
  cairo_rel_line_to(cr, grm.w + TICK_SIZE - 1, 0);
  // ticks
  for (int i = 0, x = grm.x + CELL_SIZE, y = grm.y1; i < grm.N; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, y); cairo_rel_line_to(cr, 0, TICK_SIZE);
  }
  for (int j = 0, x = grm.x, y = grm.y; j < grm.M; j++, y += CELL_SIZE) {
    cairo_move_to(cr, x, y); cairo_rel_line_to(cr, -TICK_SIZE, 0);
  }
  cairo_stroke(cr);
  // axis names
  if (!grid_pango) grid_pango = pango_cairo_create_layout(cr);
  if (grid_pango) {
    if (graph_font) pango_layout_set_font_description(grid_pango, graph_font);
    double ts = TICK_SIZE * 2;
    cairo_move_to(cr, grm.x1 + ts, grm.y1 - 0.5 * grm.fs);
    pango_layout_set_text(grid_pango, X_AXIS, -1);
    pango_cairo_show_layout(cr, grid_pango);
    cairo_move_to(cr, grm.x / 4., grm.y - (ts + 1.5 * grm.fs));
    pango_layout_set_text(grid_pango, Y_AXIS, -1);
    pango_cairo_show_layout(cr, grid_pango);
  }
}

static void gr_draw_marks(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static PangoLayout *mark_pango;
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !graph_font || (grm.y <= 0)) return;
  if (!mark_pango) mark_pango = pango_cairo_create_layout(cr);
  if (mark_pango && (grm.M > 0)) {
    if (graph_font) pango_layout_set_font_description(mark_pango, graph_font);
    pango_layout_set_width(mark_pango, (grm.x - 2 * TICK_SIZE) * PANGO_SCALE);
    pango_layout_set_alignment(mark_pango, PANGO_ALIGN_RIGHT);
    double dy = grm.ymax / GR_RTT_SCALE / grm.M;
    for (int j = grm.M, y = grm.y - 0.6 * grm.fs; j >= 0; j--, y += CELL_SIZE) {
      gchar buff[8];
      g_snprintf(buff, sizeof(buff), "%.1f", dy * j);
      cairo_move_to(cr, 0, y);
      pango_layout_set_text(mark_pango, buff, -1);
      pango_cairo_show_layout(cr, mark_pango);
    }
  }
}

static void gr_draw_mean(cairo_t *cr, int width) {
  if (!cr) return;
  if (width >= 0) cairo_set_line_width(cr, width);
  int lim = (hops_no > n_series) ? n_series : hops_no;
  for (int i = opts.min; i < lim; i++) {
    double avrg = stat_dbl_elem(i, ELEM_AVRG);
    if (avrg > 0) {
      int n = i % n_colors;
      cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], 0.6);
      double x0 = (GR_LEN > 0) ? grm.x1 - grm.dx * (GR_LEN - 1) : grm.x;
      gr_draw_line_at(cr, grm.x1, avrg, (x0 < grm.x) ? grm.x : x0, avrg);
      cairo_stroke(cr);
    }
  }
}

static void gr_draw_graph(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static PangoLayout *graph_pango;
  if (!GTK_IS_DRAWING_AREA(area) || !cr) return;
  if (!graph_pango) graph_pango = pango_cairo_create_layout(cr);
  if (graph_pango) {
    if (graph_font) pango_layout_set_font_description(graph_pango, graph_font);
    int tsz = (CELL_SIZE - TICK_SIZE) * X_FREQ;
    pango_layout_set_width(graph_pango, tsz * PANGO_SCALE);
    pango_layout_set_alignment(graph_pango, PANGO_ALIGN_CENTER);
    if (pinger_state.run || !grm.at) grm.at = time(NULL) % 3600;
    for (int i = 0, x = grm.x1 - tsz / 2, y = grm.y1 + grm.fs, t = grm.at;
        i <= grm.N; i++, x -= CELL_SIZE, t -= X_CELL_SEC) if ((i + 1) % X_FREQ) {
      gchar buff[8];
      int mm = t / 60, ss = t % 60;
      g_snprintf(buff, sizeof(buff), "%02d:%02d", mm, ss);
      cairo_move_to(cr, x, y);
      pango_layout_set_text(graph_pango, buff, -1);
      pango_cairo_show_layout(cr, graph_pango);
    }
  }
  if (opts.mean) gr_draw_mean(cr, 1);
  switch (opts.graph) {
    case GRAPH_TYPE_DOT:
      gr_draw_smth(cr, CAIRO_LINE_CAP_ROUND, DOT_SIZE, gr_draw_dot_loop);
      break;
    case GRAPH_TYPE_LINE:
      gr_draw_smth(cr, -1, LINE_SIZE, gr_draw_line_loop);
      break;
    case GRAPH_TYPE_CURVE:
      gr_draw_smth(cr, -1, LINE_SIZE, gr_draw_curve_loop);
      break;
  }
}

static void graphtab_data_update(void) {
  t_stat_graph skip = { .rtt = -1, .jttr = -1 };
  for (int i = 0; i < n_series; i++)
    gr_save(i, (pinger_state.run && (opts.min <= i) && (i < hops_no)) ? stat_graph_data_at(i) : skip);
}


// pub
//

t_tab* graphtab_init(GtkWidget* win) {
  graphtab.lab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.lab), NULL);
  graphtab.dyn = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.dyn), NULL);
  // create layers
  graph_grid = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(graph_grid), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_grid), gr_draw_grid, NULL, NULL);
  graph_marks = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(graph_marks), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_marks), gr_draw_marks, NULL, NULL);
  graph_graph = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(graph_graph), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_graph), gr_draw_graph, NULL, NULL);
  // merge layers
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(GTK_IS_OVERLAY(over), NULL);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_grid);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_marks);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_graph);
  gtk_overlay_set_child(GTK_OVERLAY(over), graphtab.dyn);
  // wrap in scroll (not necessary yet)
  graphtab.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.tab), NULL);
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(GTK_IS_SCROLLED_WINDOW(scroll), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  // put into tab
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  GtkWidget* graph_win = notifier_init(NT_GRAPH_NDX, scroll);
  gtk_box_append(GTK_BOX(graphtab.tab), graph_win ? graph_win : scroll);
  gr_set_font();
  return &graphtab;
}

void graphtab_free(void) {
  grm.at = 0; grm.ymax = GR_RTT0;
  for (int i = 0; i < n_series; i++) {
    GR_LEN = 0; if (GR_LIST) { g_slist_free(GR_LIST); GR_LIST = NULL; }
  }
}

inline void graphtab_update(void) {
  if (!pinger_state.run) return;
  if (pinger_state.gotdata && opts.legend && !notifier_get_visible(NT_GRAPH_NDX))
    notifier_set_visible(NT_GRAPH_NDX, true);
  graphtab_data_update();
  if (!pinger_state.pause) {
    if (opts.legend) notifier_legend_update(NT_GRAPH_NDX);
    gtk_widget_queue_draw(graph_graph);
  }
}

inline void graphtab_force_update(void) { gtk_widget_queue_draw(graph_graph); }
inline void graphtab_toggle_legend(void) { if (opts.graph) notifier_set_visible(NT_GRAPH_NDX, opts.legend); }

