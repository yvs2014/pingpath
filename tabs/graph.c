
#include "graph.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/notifier.h"

#define GRFONT_RATIO 0.28

#define GR_Y_GAP 1.1

#define TICK_SIZE 6
#define DOT_SIZE  5
#define LINE_SIZE 2
#define THIN_SIZE 1
#define JRNG_MIN  4  // LINE_SIZE * 2

#define LINE_ALPHA 0.6
#define AREA_ALPHA 0.2

#define X_IN_TIME_TICKS 5
#define X_FREQ          2
#define X_JRNG_TICK     2

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

typedef struct gr_color { double ondark, onlight; } t_gr_color;

typedef void (*draw_smth_fn)(int i, cairo_t *cr, double x);

static const t_gr_color dash_col = { .ondark = 0.85, .onlight = 0.7 };
static const t_gr_color text_col = { .ondark = 0.85, .onlight = 0   };
#define GR_SETCOL(gcol) { double col = opts.darkgraph ? gcol.ondark : gcol.onlight; \
  cairo_set_source_rgb(cr, col, col, col); }
#define GR_TEXTCOL GR_SETCOL(text_col)

static t_tab graphtab = { .self = &graphtab, .name = "graph-tab",
  .tag = GRAPH_TAB_TAG, .tip = GRAPH_TAB_TIP, .ico = {GRAPH_TAB_ICON, GRAPH_TAB_ICOA, GRAPH_TAB_ICOB},
};

static t_gr_list gr_series[MAXTTL], gr_series_kept[MAXTTL];
#define GR_LIST (gr_series[i].list)
#define GR_LEN (gr_series[i].len)
#define GR_LIST_CP (gr_series_kept[i].list)
#define GR_LEN_CP (gr_series_kept[i].len)
static const int n_series = G_N_ELEMENTS(gr_series);
static gboolean gr_series_lock;

static GtkWidget *graph_grid, *graph_marks, *graph_graph;
static PangoFontDescription *graph_font;
static PangoLayout *grid_pango, *mark_pango, *graph_pango;

static t_gr_aux_measures grm = { .x = GRAPH_LEFT, .y = GRAPH_TOP, .fs = CELL_SIZE * GRFONT_RATIO, .ymax = PP_RTT0,
  .dx = MAIN_TIMING_SEC * ((double)CELL_SIZE / X_IN_TIME_TICKS),
  .no = X_RES / ((double)CELL_SIZE / X_IN_TIME_TICKS) + 1, // to start keeping data w/o knowing drawing area width
};

//

static void gr_scale_max(int max) {
  grm.ymax = max * GR_Y_GAP;
  gtk_widget_queue_draw(graph_marks);
  gtk_widget_set_visible(graph_marks, true);
}

static int gr_max_in_series(void) {
  int max = PP_RTT0;
  for (int i = 0; i < n_series; i++) {
    for (GSList *item = GR_LIST; item; item = item->next) {
      if (!item->data) continue;
      int rtt = ((t_rseq*)item->data)->rtt;
      if (rtt > max) max = rtt;
    }
  }
  return max;
}

#define GR_ACTUAL_LIST (series[i].list)
#define GR_ACTUAL_LEN (series[i].len)

static void gr_save(int i, t_rseq *data) {
  if (gr_series_lock || !data) return;
  t_rseq *copy = g_memdup2(data, sizeof(*data));
  if (!copy) return; // unlikely
  t_gr_list* series = pinger_state.pause ? gr_series_kept : gr_series;
  GR_ACTUAL_LIST = g_slist_prepend(GR_ACTUAL_LIST, copy); GR_ACTUAL_LEN++;
  if (data->rtt > grm.ymax) gr_scale_max(data->rtt); // up-scale
  while (GR_ACTUAL_LIST && (GR_ACTUAL_LEN > grm.no)) {
    GSList *last = g_slist_last(GR_ACTUAL_LIST);
    if (!last) break;
    int rtt = last->data ? ((t_rseq*)last->data)->rtt : -1;
    if (last->data) { g_free(last->data); last->data = NULL; }
    GR_ACTUAL_LIST = g_slist_delete_link(GR_ACTUAL_LIST, last); GR_ACTUAL_LEN--;
    if ((rtt * GR_Y_GAP) >= grm.ymax) {
      int max = gr_max_in_series();
      if (((max * GR_Y_GAP) < grm.ymax) && (max > PP_RTT0)) gr_scale_max(max); // down-scale
    }
  }
}

static void gr_set_font(void) {
  if (graph_font) pango_font_description_free(graph_font);
  graph_font = pango_font_description_new();
  if (graph_font) {
    pango_font_description_set_family(graph_font, PP_FONT_FAMILY);
    pango_font_description_set_absolute_size(graph_font, (grm.fs ? grm.fs : (CELL_SIZE * GRFONT_RATIO)) * PANGO_SCALE);
  } else WARN_("Cannot allocate pango font");
}

static inline double rtt2y(double rtt) { return grm.y1 - rtt / grm.ymax * grm.h; }
static inline double yscaled(double rtt) { return rtt / grm.ymax * grm.h; }

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

static void gr_draw_dot_loop(int i, cairo_t *cr, double x) {
  for (GSList *item = GR_LIST; item; item = item->next, x -= grm.dx) {
    if (x < grm.x) break;
    t_rseq *data = item->data;
    if (IS_RTT_DATA(data)) gr_draw_dot_at(cr, x, data->rtt);
  }
}

static void gr_draw_dot_scope_loop(int i, cairo_t *cr, double x) {
  double dy = stat_dbl_elem(i, ELEM_JTTR); if (dy <= 0) return;
  dy = yscaled(dy); if (dy <= JRNG_MIN) return;
  for (GSList *item = GR_LIST; item; item = item->next, x -= grm.dx) {
    if (x < grm.x) break;
    t_rseq *data = item->data;
    if (IS_RTT_DATA(data)) {
      double x0 = x - X_JRNG_TICK; if (x0 < grm.x)  x0 = grm.x;
      double x1 = x + X_JRNG_TICK; if (x1 > grm.x1) x1 = grm.x1;
      double y = rtt2y(data->rtt);
      double y0 = y - dy; if (y0 < grm.y)  y0 = grm.y;
      double y1 = y + dy; if (y1 > grm.y1) y1 = grm.y1;
      cairo_move_to(cr, x,  y0); cairo_line_to(cr, x,  y1);
      cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y0);
      cairo_move_to(cr, x0, y1); cairo_line_to(cr, x1, y1);
    }
  }
}

#define FULL_DRAW_DOT(x, rtt) { cairo_stroke(cr); cairo_set_line_width(cr, DOT_SIZE); \
  gr_draw_dot_at(cr, x, rtt); cairo_stroke(cr); cairo_set_line_width(cr, line_width); }

static void gr_draw_line_loop(int i, cairo_t *cr, double x) {
  double line_width = cairo_get_line_width(cr);
  t_gr_point_desc prev = { .rtt = 0, .conn = true };
  for (GSList *item = GR_LIST; item; item = item->next, x -= grm.dx) {
    if (x < grm.x) break;
    t_rseq *data = item->data;
    if (!data) continue;
    gboolean connected = false;
    if (prev.rtt > 0) {
      if (data->rtt > 0) {
        gr_draw_line_at(cr, prev.x, prev.rtt, x, data->rtt);
        connected = true;
      } else if (!prev.conn) FULL_DRAW_DOT(prev.x, prev.rtt);
    }
    prev.x = x; prev.rtt = data->rtt; prev.conn = connected;
  }
}

static inline double distance(int x0, int y0, int x1, int y1) {
  int dx1 = x1 - x0; int dy1 = y1 - y0;
  return sqrt(dx1 * dx1 + dy1 * dy1);
}

static inline int centripetal(double d1, double q1, double d2, double q2, int p0, int p1, int p2) {
  return (d1 * p0 - d2 * p1 + (2 * d1 + 3 * q1 * q2 + d2) * p2) / (3 * q1 * (q1 + q2)) + 0.5;
}

#define DRAW_CURVE_PROLOG { if (GR_LIST) { t_rseq *data = GR_LIST->data; int r0 = RTT_OR_NEG(data); \
  if (r0 > 0) { data = g_slist_nth_data(GR_LIST, 1); int r1 = RTT_OR_NEG(data); \
    if (r1 > 0) gr_draw_line_at(cr, x0, r0, x0 - grm.dx, r1); else FULL_DRAW_DOT(x0, r0); }}}

static void gr_draw_curve_loop(int i, cairo_t *cr, double x0) {
  double line_width = cairo_get_line_width(cr);
  DRAW_CURVE_PROLOG;
  for (GSList *item = GR_LIST; item; item = item->next, x0 -= grm.dx) {
    int r0, r1, r2, r3;
    double x1, x2, x3;
    t_rseq *data = item->data;        r0 = RTT_OR_NEG(data);
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
    else if ((r1 > 0) && !(r0 > 0)) FULL_DRAW_DOT(x1, r1);
  }
}

#define SKIP_EXCLUDED { if (lgnd_excl_mask && (lgnd_excl_mask & (1 << n))) continue; }

static void gr_draw_smth(cairo_t *cr, double width, double a, draw_smth_fn draw) {
  if (!cr || !draw) return;
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  if (width >= 0) cairo_set_line_width(cr, width);
  int lim = (hops_no > n_series) ? n_series : hops_no;
  for (int i = opts.min; i < lim; i++) {
    int n = i % n_colors;
    SKIP_EXCLUDED;
    if (a < 0) cairo_set_source_rgb(cr, colors[n][0], colors[n][1], colors[n][2]);
    else cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], a);
    draw(i, cr, grm.x1);
    cairo_stroke(cr);
  }
}

static void gr_draw_grid(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  const double dash_size[] = {1};
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !w || !h) return;
  int w0 = w - (GRAPH_LEFT + GRAPH_RIGHT), h0 = h - (GRAPH_TOP + GRAPH_BOTTOM);
  grm.N = w0 / CELL_SIZE, grm.M = h0 / CELL_SIZE;
  grm.w = grm.N * CELL_SIZE, grm.h = grm.M * CELL_SIZE;
  grm.no = grm.w / ((double)CELL_SIZE / X_IN_TIME_TICKS) + 1; // number elems per list
  grm.x1 = grm.x + grm.w; grm.y1 = grm.y + grm.h;
  cairo_set_line_width(cr, opts.darkgraph ? (LINE_SIZE * 0.6) : LINE_SIZE);
  // grid lines
  cairo_set_dash(cr, dash_size, 1, 0);
  GR_SETCOL(dash_col);
  for (int i = 0, x = grm.x + CELL_SIZE; i < grm.N; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, grm.y);
    cairo_rel_line_to(cr, 0, grm.h);
  }
  for (int j = 0, y = grm.y; j < grm.M; j++, y += CELL_SIZE) {
    cairo_move_to(cr, grm.x, y);
    cairo_rel_line_to(cr, grm.w, 0);
  }
  cairo_stroke(cr);
  cairo_set_line_width(cr, opts.darkgraph ? (LINE_SIZE * 0.7) : LINE_SIZE);
  // main axes
  cairo_set_dash(cr, dash_size, 0, 0);
  GR_TEXTCOL;
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
  if (!grid_pango) {
    grid_pango = pango_cairo_create_layout(cr);
    if (grid_pango) {
      if (graph_font) pango_layout_set_font_description(grid_pango, graph_font);
    } else FAIL("pango_cairo_create_layout()");
  }
  if (grid_pango) {
    double ts = TICK_SIZE * 2;
    cairo_move_to(cr, grm.x1 + ts, grm.y1 - 0.5 * grm.fs);
    pango_layout_set_text(grid_pango, X_AXIS_TITLE, -1);
    pango_cairo_show_layout(cr, grid_pango);
    cairo_move_to(cr, grm.x / 4., grm.y - (ts + 1.5 * grm.fs));
    pango_layout_set_text(grid_pango, Y_AXIS_TITLE, -1);
    pango_cairo_show_layout(cr, grid_pango);
  }
}

static void gr_draw_marks(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !graph_font || (grm.y <= 0)) return;
  if (!mark_pango) {
    mark_pango = pango_cairo_create_layout(cr);
    if (mark_pango) {
      if (graph_font) pango_layout_set_font_description(mark_pango, graph_font);
      pango_layout_set_width(mark_pango, (grm.x - 2 * TICK_SIZE) * PANGO_SCALE);
      pango_layout_set_alignment(mark_pango, PANGO_ALIGN_RIGHT);
    } else FAIL("pango_cairo_create_layout()");
  }
  if (mark_pango) {
    if (grm.M > 0) {
      GR_TEXTCOL;
      gchar buff[16];
      double dy = grm.ymax / PP_RTT_SCALE / grm.M;
      for (int j = grm.M, y = grm.y - 0.6 * grm.fs; j >= 0; j--, y += CELL_SIZE) {
        g_snprintf(buff, sizeof(buff), PP_RTT_AXIS_FMT, dy * j);
        cairo_move_to(cr, 0, y);
        pango_layout_set_text(mark_pango, buff, -1);
        pango_cairo_show_layout(cr, mark_pango);
      }
    }
  }
}

static void gr_draw_mean(cairo_t *cr, gboolean mean, gboolean area) {
  if (!cr) return;
  int lim = (hops_no > n_series) ? n_series : hops_no;
  for (int i = opts.min; i < lim; i++) {
    int n = i % n_colors;
    SKIP_EXCLUDED;
    double y = stat_dbl_elem(i, ELEM_AVRG); if (y <= 0) continue;
    y = rtt2y(y);
    double x0 = (GR_LEN > 0) ? grm.x1 - grm.dx * (GR_LEN - 1) : grm.x; if (x0 < grm.x) x0 = grm.x;
    if (area) {
      double dy = stat_dbl_elem(i, ELEM_JTTR);
      if (dy > 0) {
        dy = yscaled(dy);
        if (dy > JRNG_MIN) {
          cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], AREA_ALPHA);
          double y0 = y - dy; if (y0 < grm.y)  y0 = grm.y;
          double y1 = y + dy; if (y1 > grm.y1) y1 = grm.y1;
          cairo_move_to(cr, x0, y0);
          cairo_line_to(cr, grm.x1, y0);
          cairo_line_to(cr, grm.x1, y1);
          cairo_line_to(cr, x0, y1);
          cairo_close_path(cr);
          cairo_fill(cr);
        }
      }
    }
    if (mean) {
      cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], LINE_ALPHA);
      cairo_set_line_width(cr, THIN_SIZE);
      cairo_move_to(cr, grm.x1, y);
      cairo_line_to(cr, x0, y);
      cairo_stroke(cr);
    }
  }
}

static void gr_draw_graph(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static const int graph_tsz = (CELL_SIZE - TICK_SIZE) * X_FREQ;
  if (!GTK_IS_DRAWING_AREA(area) || !cr) return;
  if (!graph_pango) {
    graph_pango = pango_cairo_create_layout(cr);
    if (graph_pango) {
      if (graph_font) pango_layout_set_font_description(graph_pango, graph_font);
      pango_layout_set_width(graph_pango, graph_tsz * PANGO_SCALE);
      pango_layout_set_alignment(graph_pango, PANGO_ALIGN_CENTER);
    } // else FAIL("pango_cairo_create_layout()");
  }
  if (graph_pango) {
    GR_TEXTCOL;
    if (!grm.at || (pinger_state.run && !pinger_state.pause)) grm.at = time(NULL) % 3600;
    int dt = X_IN_TIME_TICKS; if (opts.timeout > 0) dt *= opts.timeout;
    gchar buff[8];
    for (int i = 0, x = grm.x1 - graph_tsz / 2, y = grm.y1 + grm.fs, t = grm.at;
        i <= grm.N; i++, x -= CELL_SIZE, t -= dt) if ((i + 1) % X_FREQ) {
      if (t < 0) t += 3600;
      int mm = t / 60, ss = t % 60;
      g_snprintf(buff, sizeof(buff), PP_TIME_AXIS_FMT, mm, ss);
      cairo_move_to(cr, x, y);
      pango_layout_set_text(graph_pango, buff, -1);
      pango_cairo_show_layout(cr, graph_pango);
    }
  }
  { gboolean mean = is_grelem_enabled(GREX_MEAN), area = is_grelem_enabled(GREX_AREA);
    if (mean || area) gr_draw_mean(cr, mean, area);
    if (is_grelem_enabled(GREX_JRNG)) gr_draw_smth(cr, THIN_SIZE, LINE_ALPHA, gr_draw_dot_scope_loop);
  }
  switch (opts.graph) {
    case GRAPH_TYPE_DOT:
      gr_draw_smth(cr, DOT_SIZE, -1, gr_draw_dot_loop);
      break;
    case GRAPH_TYPE_LINE:
      gr_draw_smth(cr, LINE_SIZE, -1, gr_draw_line_loop);
      break;
    case GRAPH_TYPE_CURVE:
      gr_draw_smth(cr, LINE_SIZE, -1, gr_draw_curve_loop);
      break;
  }
}

#define GR_LAST_SEQ ((GR_ACTUAL_LEN && GR_ACTUAL_LIST && GR_ACTUAL_LIST->data) ? ((t_rseq*)GR_ACTUAL_LIST->data)->seq : 0)

static void graphtab_data_update(void) {
  if (gr_series_lock) return;
  t_gr_list* series = pinger_state.pause ? gr_series_kept : gr_series;
  t_rseq skip = { .rtt = -1, .seq = 0 };
  for (int i = 0; i < n_series; i++) {
    if ((opts.min <= i) && (i < hops_no)) {
      int seq = GR_LAST_SEQ + 1;
      t_rseq data = { .seq = seq };
      stat_rseq(i, &data);
      if (data.seq > 0) gr_save(i, &data);
      else /* sync seq */ if (data.seq < 0) { // rarely
        data.seq = -data.seq;
        if (data.seq < seq) DEBUG("sync back: req#%d got#%d", seq, data.seq)
        else { // unlikely, just in case
          gr_save(i, &data);
          DEBUG("sync forward: req#%d got#%d", seq, data.seq);
        }
      }
    } else gr_save(i, &skip);
  }
}

gpointer gr_dup_stat_graph(gconstpointer src, gpointer data) { return g_memdup2(src, sizeof(t_rseq)); }

static void gr_freeze_data(void) {
  gr_series_lock = true;
  for (int i = 0; i < n_series; i++) { // keep current data
    GR_LIST_CP = GR_LIST ? g_slist_copy_deep(GR_LIST, gr_dup_stat_graph, NULL) : NULL;
    GR_LEN_CP = g_slist_length(GR_LIST_CP);
  }
  gr_series_lock = false;
}

static void gr_unfreeze_data(void) {
  gr_series_lock = true;
  for (int i = 0; i < n_series; i++) { // clean current data, copy kept references
    if (GR_LIST) g_slist_free_full(GR_LIST, g_free);
    GR_LIST = GR_LIST_CP; GR_LEN = GR_LEN_CP;
    GR_LEN_CP = 0; GR_LIST_CP = NULL;
  }
  gr_series_lock = false;
}


// pub
//

t_tab* graphtab_init(GtkWidget* win) {
  TW_TW(graphtab.lab, gtk_box_new(GTK_ORIENTATION_VERTICAL, 2), CSS_PAD, NULL);
  g_return_val_if_fail(graphtab.lab.w, NULL);
  TW_TW(graphtab.dyn, gtk_box_new(GTK_ORIENTATION_VERTICAL, 0), NULL, CSS_GR_BG);
  g_return_val_if_fail(graphtab.dyn.w, NULL);
  // create layers
  graph_grid = gtk_drawing_area_new();
  g_return_val_if_fail(graph_grid, NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_grid), gr_draw_grid, NULL, NULL);
  graph_marks = gtk_drawing_area_new();
  g_return_val_if_fail(graph_marks, NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_marks), gr_draw_marks, NULL, NULL);
  graph_graph = gtk_drawing_area_new();
  g_return_val_if_fail(graph_graph, NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph_graph), gr_draw_graph, NULL, NULL);
  // merge layers
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(over, NULL);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_grid);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_marks);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), graph_graph);
  gtk_overlay_set_child(GTK_OVERLAY(over), graphtab.dyn.w);
  // wrap scrolling
  graphtab.tab.w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(graphtab.tab.w, NULL);
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(scroll, NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  // put into tab
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  GtkWidget* graph_win = notifier_init(NT_GRAPH_NDX, scroll);
  gtk_box_append(GTK_BOX(graphtab.tab.w), graph_win ? graph_win : scroll);
  gr_set_font();
  return &graphtab;
}

void graphtab_free(gboolean finish) {
  grm.at = 0; grm.ymax = PP_RTT0;
  for (int i = 0; i < n_series; i++) {
    GR_LEN = 0; if (GR_LIST) { g_slist_free_full(GR_LIST, g_free); GR_LIST = NULL; }
    GR_LEN_CP = 0; if (GR_LIST_CP) { g_slist_free_full(GR_LIST_CP, g_free); GR_LIST_CP = NULL; }
  }
  if (!finish) return;
  if (grid_pango)  { g_object_unref(grid_pango);  grid_pango = NULL; }
  if (mark_pango)  { g_object_unref(mark_pango);  mark_pango = NULL; }
  if (graph_pango) { g_object_unref(graph_pango); graph_pango = NULL; }
  if (graph_font)  { pango_font_description_free(graph_font); graph_font = NULL; }
  pango_cairo_font_map_set_default(NULL); // a bit less
}

#define GRAPH_VIEW_UPDATE { if (!pinger_state.pause) { \
  if (opts.legend) notifier_legend_update(); \
  if (GTK_IS_WIDGET(graph_graph)) gtk_widget_queue_draw(graph_graph); \
}}

void graphtab_update(gboolean retrieve) {
  if (!pinger_state.run) return;
  if (retrieve) graphtab_data_update();
  GRAPH_VIEW_UPDATE;
}

void graphtab_final_update(void) { graphtab_data_update(); GRAPH_VIEW_UPDATE; }
void graphtab_toggle_legend(void) { if (opts.graph) notifier_set_visible(NT_GRAPH_NDX, opts.legend); }

void graphtab_graph_refresh(gboolean pause_toggled) {
  if (GTK_IS_WIDGET(graph_graph)) gtk_widget_queue_draw(graph_graph);
  if (pause_toggled) { if (pinger_state.pause) gr_freeze_data(); else gr_unfreeze_data(); }
}

void graphtab_full_refresh(void) {
  if (!opts.graph) return;
  if (GTK_IS_WIDGET(graph_grid))  gtk_widget_queue_draw(graph_grid);
  if (GTK_IS_WIDGET(graph_marks)) gtk_widget_queue_draw(graph_marks);
  if (GTK_IS_WIDGET(graph_graph)) gtk_widget_queue_draw(graph_graph);
  notifier_legend_reload_css();
}

