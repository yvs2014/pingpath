
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "common.h"

#include "graph.h"
#include "aux.h"
#include "series.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"
#include "ui/notifier.h"

#define GR_XAXIS_TITLE "Time"
#define GR_YAXIS_TITLE "Delay [msec]"

enum {
  CELL_SIZE = 50,
  TICK_SIZE =  6,
  DOT_SIZE  =  5,
  LINE_SIZE =  2,
  THIN_SIZE =  1,
  JRNG_MIN  =  4,  // LINE_SIZE * 2
};

static const double FONT_RATIO = 0.28;
static const double LINE_ALPHA = 0.6;
static const double AREA_ALPHA = 0.2;
enum { SUBCELLS = 5, X_FREQ = 2, SCOPE_TICK = 2 };

typedef struct graph_geom {
  const int x0, y0;
  int x1, y1, w, h, nw, nh;
} t_graph_geom;

typedef struct gr_point_desc {
  double x;
  int rtt;
  gboolean conn;
} t_gr_point_desc;

typedef void (*draw_smth_fn)(int ttl, cairo_t *cr, double x);

static const t_th_color dash_col = { .ondark = 0.85, .onlight = 0.7 };
static const t_th_color text_col = { .ondark = 0.85, .onlight = 0   };
#define GR_SETCOL(thcol) { double col = opts.darkgraph ? (thcol).ondark : (thcol).onlight; \
  cairo_set_source_rgb(cr, col, col, col); }
#define GR_TEXTCOL GR_SETCOL(text_col)

static t_graph_geom grm = { .x0 = GRAPH_LEFT, .y0 = GRAPH_TOP };
static const double fs_size = CELL_SIZE * FONT_RATIO;
static const double dx_step = MAIN_TIMING_SEC * ((double)CELL_SIZE / SUBCELLS);

static t_tab graphtab = { .self = &graphtab, .name = "graph-tab",
  .tag = GRAPH_TAB_TAG, .tip = GRAPH_TAB_TIP, .ico = {GRAPH_TAB_ICON, GRAPH_TAB_ICOA, GRAPH_TAB_ICOB},
};

static GtkWidget *graph_grid, *graph_marks, *graph_graph;
static PangoFontDescription *graph_font;
static PangoLayout *grid_pango, *mark_pango, *graph_pango;

static int draw_graph_at;

//

static void gr_set_font(void) {
  if (graph_font) pango_font_description_free(graph_font);
  graph_font = pango_font_description_new();
  if (graph_font) {
    pango_font_description_set_family(graph_font, PP_FONT_FAMILY);
    if (fs_size) pango_font_description_set_absolute_size(graph_font, fs_size * PANGO_SCALE);
  } else WARN("Cannot allocate pango font");
}

static inline double rtt2y(double rtt) { return grm.y1 - rtt / series_datamax * grm.h; }
static inline double yscaled(double rtt) { return rtt / series_datamax * grm.h; }

static inline void gr_draw_dot_at(cairo_t *cr, double x, double rtt) {
  cairo_move_to(cr, x, rtt2y(rtt));
  cairo_close_path(cr);
}

static inline void gr_draw_line_at(cairo_t *cr, double a, double a_rtt, double b, double b_rtt) {
  cairo_move_to(cr, a, rtt2y(a_rtt));
  cairo_line_to(cr, b, rtt2y(b_rtt));
}

static void gr_draw_dot_loop(int ttl, cairo_t *cr, double x) {
  for (GSList *item = SERIES(ttl); item; item = item->next) {
    if (x < grm.x0) break;
    t_rseq *data = item->data;
    if (IS_RTT_DATA(data)) gr_draw_dot_at(cr, x, data->rtt);
    x -= dx_step;
  }
}

static void gr_draw_dot_scope_loop(int ttl, cairo_t *cr, double x) {
  double dy = stat_dbl_elem(ttl, PE_JTTR); if (dy <= 0) return;
  dy = yscaled(dy); if (dy <= JRNG_MIN) return;
  for (GSList *item = SERIES(ttl); item; item = item->next) {
    if (x < grm.x0) break;
    t_rseq *data = item->data;
    if (IS_RTT_DATA(data)) {
      double x0 = x - SCOPE_TICK; if (x0 < grm.x0) x0 = grm.x0;
      double x1 = x + SCOPE_TICK; if (x1 > grm.x1) x1 = grm.x1;
      double y = rtt2y(data->rtt);
      double y0 = y - dy; if (y0 < grm.y0) y0 = grm.y0;
      double y1 = y + dy; if (y1 > grm.y1) y1 = grm.y1;
      cairo_move_to(cr, x,  y0); cairo_line_to(cr, x,  y1);
      cairo_move_to(cr, x0, y0); cairo_line_to(cr, x1, y0);
      cairo_move_to(cr, x0, y1); cairo_line_to(cr, x1, y1);
    }
    x -= dx_step;
  }
}

#define GRAPH_STROKE_DOT(x, rtt) { cairo_stroke(cr); cairo_set_line_width(cr, DOT_SIZE); \
  gr_draw_dot_at(cr, x, rtt); cairo_stroke(cr); cairo_set_line_width(cr, line_width); }

static void gr_draw_line_loop(int ttl, cairo_t *cr, double x) {
  double line_width = cairo_get_line_width(cr);
  t_gr_point_desc prev = { .rtt = 0, .conn = true };
  for (GSList *item = SERIES(ttl); item; item = item->next) {
    if (x < grm.x0) break;
    t_rseq *data = item->data;
    if (data) {
      gboolean connected = false;
      if (prev.rtt > 0) {
        if (data->rtt > 0) {
          gr_draw_line_at(cr, prev.x, prev.rtt, x, data->rtt);
          connected = true;
        } else if (!prev.conn) GRAPH_STROKE_DOT(prev.x, prev.rtt);
      }
      prev.x = x; prev.rtt = data->rtt; prev.conn = connected;
    }
    x -= dx_step;
  }
}

static inline double distance(int x0, int y0, int x1, int y1) {
  int dx1 = x1 - x0; int dy1 = y1 - y0;
  return sqrt(dx1 * dx1 + dy1 * dy1);
}

static inline int centripetal(double d1, double q1, double d2, double q2, int p0, int p1, int p2) {
  return (d1 * p0 - d2 * p1 + (2 * d1 + 3 * q1 * q2 + d2) * p2) / (3 * q1 * (q1 + q2)) + 0.5;
}

static void gr_draw_curve_loop(int ttl, cairo_t *cr, double x0) {
#define RTT_OR_NEG(d) (IS_RTT_DATA(d) ? (d)->rtt : -1)
  if (!SERIES(ttl)) return;
  double line_width = cairo_get_line_width(cr);
  { t_rseq *data = SERIES(ttl)->data; int rtt0 = RTT_OR_NEG(data);
    if (rtt0 > 0) {
      data = g_slist_nth_data(SERIES(ttl), 1); int rtt1 = RTT_OR_NEG(data);
      if (rtt1 > 0) gr_draw_line_at(cr, x0, rtt0, x0 - dx_step, rtt1);
      else GRAPH_STROKE_DOT(x0, rtt0);
    }
  }
  for (GSList *item = SERIES(ttl); item; item = item->next) {
    t_rseq *data = item->data;        int rtt0 = RTT_OR_NEG(data);
    data = g_slist_nth_data(item, 1); int rtt1 = RTT_OR_NEG(data); double x1 = x0 - dx_step;
    data = g_slist_nth_data(item, 2); int rtt2 = RTT_OR_NEG(data); double x2 = x1 - dx_step;
    data = g_slist_nth_data(item, 3); int rtt3 = RTT_OR_NEG(data); double x3 = x2 - dx_step;
    if (x2 < grm.x0) break;
    if ((rtt0 > 0) && (rtt1 > 0) && (rtt2 > 0) && (rtt3 > 0)) {
      double y0 = rtt2y(rtt0), y1 = rtt2y(rtt1), y2 = rtt2y(rtt2), y3 = rtt2y(rtt3);
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
    } else if ((rtt1 > 0) && (rtt2 > 0)) gr_draw_line_at(cr, x1, rtt1, x2, rtt2);
    else if ((rtt1 > 0) && !(rtt0 > 0)) GRAPH_STROKE_DOT(x1, rtt1);
    //
    x0 -= dx_step;
  }
#undef RTT_OR_NEG
}

#define SKIP_EXCLUDED { if (lgnd_excl_mask && (lgnd_excl_mask & (1U << n))) continue; }

static void gr_draw_smth(cairo_t *cr, double width, double alpha, draw_smth_fn draw) {
  if (!cr || !draw) return;
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  if (width >= 0) cairo_set_line_width(cr, width);
  int lim = SERIES_LIM;
  for (int i = opts.min; i < lim; i++) {
    unsigned n = i % n_colors;
    SKIP_EXCLUDED;
    if (alpha < 0) cairo_set_source_rgb(cr, colors[n][0], colors[n][1], colors[n][2]);
    else cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], alpha);
    draw(i, cr, grm.x1);
    cairo_stroke(cr);
  }
}

static void gr_draw_grid(GtkDrawingArea *area, cairo_t* cr, int width, int height, void *unused) {
  const double dash_size[] = {1};
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !width || !height) return;
  int w0 = width - (GRAPH_LEFT + GRAPH_RIGHT), h0 = height - (GRAPH_TOP + GRAPH_BOTTOM);
  grm.nw = w0 / CELL_SIZE; grm.w = grm.nw * CELL_SIZE;
  grm.nh = h0 / CELL_SIZE; grm.h = grm.nh * CELL_SIZE;
  series_min_no(grm.w / ((double)CELL_SIZE / SUBCELLS) + 1);
  grm.x1 = grm.x0 + grm.w; grm.y1 = grm.y0 + grm.h;
  cairo_set_line_width(cr, opts.darkgraph ? (LINE_SIZE * 0.6) : LINE_SIZE);
  // grid lines
  cairo_set_dash(cr, dash_size, 1, 0);
  GR_SETCOL(dash_col);
  for (int i = 0, x = grm.x0 + CELL_SIZE; i < grm.nw; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, grm.y0);
    cairo_rel_line_to(cr, 0, grm.h);
  }
  for (int j = 0, y = grm.y0; j < grm.nh; j++, y += CELL_SIZE) {
    cairo_move_to(cr, grm.x0, y);
    cairo_rel_line_to(cr, grm.w, 0);
  }
  cairo_stroke(cr);
  cairo_set_line_width(cr, opts.darkgraph ? (LINE_SIZE * 0.7) : LINE_SIZE);
  // main axes
  cairo_set_dash(cr, dash_size, 0, 0);
  GR_TEXTCOL;
#define TICK_SIZE_1 (TICK_SIZE + 1)
  cairo_move_to(cr, grm.x0, grm.y0 - TICK_SIZE_1);
  cairo_rel_line_to(cr, 0, grm.h + TICK_SIZE_1);
  cairo_rel_line_to(cr, grm.w + TICK_SIZE - 1, 0);
#undef TICK_SIZE_1
  // ticks
  for (int i = 0, x = grm.x0 + CELL_SIZE, y = grm.y1; i < grm.nw; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, y); cairo_rel_line_to(cr, 0, TICK_SIZE);
  }
  for (int j = 0, x = grm.x0, y = grm.y0; j < grm.nh; j++, y += CELL_SIZE) {
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
#define TICK_SIZE2 (TICK_SIZE * 2)
    cairo_move_to(cr, grm.x1 + TICK_SIZE2, grm.y1 - 0.5 * fs_size);
    pango_layout_set_text(grid_pango, GR_XAXIS_TITLE, -1);
    pango_cairo_show_layout(cr, grid_pango);
    cairo_move_to(cr, grm.x0 / 4., grm.y0 - (TICK_SIZE2 + 1.5 * fs_size));
    pango_layout_set_text(grid_pango, GR_YAXIS_TITLE, -1);
    pango_cairo_show_layout(cr, grid_pango);
#undef TICK_SIZE2
  }
}

static void gr_draw_marks(GtkDrawingArea *area, cairo_t* cr, int width, int height, void *unused) {
  if (!GTK_IS_DRAWING_AREA(area) || !cr || !graph_font || (grm.y0 <= 0)) return;
  if (!mark_pango) {
    mark_pango = pango_cairo_create_layout(cr);
    if (mark_pango) {
      if (graph_font) pango_layout_set_font_description(mark_pango, graph_font);
      pango_layout_set_width(mark_pango, (grm.x0 - 2 * TICK_SIZE) * PANGO_SCALE);
      pango_layout_set_alignment(mark_pango, PANGO_ALIGN_RIGHT);
    } else FAIL("pango_cairo_create_layout()");
  }
  if (mark_pango) {
    if (grm.nh > 0) {
      GR_TEXTCOL;
      char buff[PP_MARK_MAXLEN];
      double dy = series_datamax / PP_RTT_SCALE / grm.nh;
      for (int j = grm.nh, y = grm.y0 - 0.6 * fs_size; j >= 0; j--, y += CELL_SIZE) {
        double val = dy * j;
        snprintf(buff, sizeof(buff), PP_FMT10(val), val); // BUFFNOLINT
        cairo_move_to(cr, 0, y);
        pango_layout_set_text(mark_pango, buff, -1);
        pango_cairo_show_layout(cr, mark_pango);
      }
    }
  }
}

static void gr_draw_mean(cairo_t *cr, gboolean mean, gboolean area) {
  if (!cr) return;
  int lim = SERIES_LIM;
  for (int i = opts.min; i < lim; i++) {
    unsigned n = i % n_colors;
    SKIP_EXCLUDED;
    double y = stat_dbl_elem(i, PE_AVRG); if (y <= 0) continue;
    y = rtt2y(y);
    double x0 = (SERIES_LEN(i) > 0) ? grm.x1 - dx_step * (SERIES_LEN(i) - 1) : grm.x0;
    if (x0 < grm.x0) x0 = grm.x0;
    if (area) {
      double dy = stat_dbl_elem(i, PE_JTTR);
      if (dy > 0) {
        dy = yscaled(dy);
        if (dy > JRNG_MIN) {
          cairo_set_source_rgba(cr, colors[n][0], colors[n][1], colors[n][2], AREA_ALPHA);
          double y0 = y - dy; if (y0 < grm.y0) y0 = grm.y0;
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

static void gr_draw_graph(GtkDrawingArea *area, cairo_t* cr, int width, int height, void *unused) {
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
    if (!draw_graph_at || (pinger_state.run && !pinger_state.pause)) draw_graph_at = time(NULL) % 3600;
    int dt = SUBCELLS; if (opts.timeout > 0) dt *= opts.timeout;
    char buff[PP_MARK_MAXLEN];
    for (int i = 0, x = grm.x1 - graph_tsz / 2, y = grm.y1 + fs_size, t = draw_graph_at;
        i <= grm.nw; i++, x -= CELL_SIZE, t -= dt) if ((i + 1) % X_FREQ) {
      LIMVAL(t, 3600);
      snprintf(buff, sizeof(buff), PP_TIME_FMT, t / 60, t % 60); // BUFFNOLINT
      cairo_move_to(cr, x, y);
      pango_layout_set_text(graph_pango, buff, -1);
      pango_cairo_show_layout(cr, graph_pango);
    }
  }
  { gboolean mean = is_grelem_enabled(GX_MEAN), area = is_grelem_enabled(GX_AREA);
    if (mean || area) gr_draw_mean(cr, mean, area);
    if (is_grelem_enabled(GX_JRNG)) gr_draw_smth(cr, THIN_SIZE, LINE_ALPHA, gr_draw_dot_scope_loop);
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
    default: break;
  }
}


// pub
//

t_tab* graphtab_init(void) {
#define GR_INIT_LAYER(widget, drawfunc) { (widget) = gtk_drawing_area_new(); g_return_val_if_fail(widget, NULL); \
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(widget), drawfunc, NULL, NULL); \
    layers = g_slist_append(layers, widget); }
  { GSList *layers = NULL;
    GR_INIT_LAYER(graph_grid,  gr_draw_grid);
    GR_INIT_LAYER(graph_marks, gr_draw_marks);
    GR_INIT_LAYER(graph_graph, gr_draw_graph);
    if (!drawtab_init(&graphtab, CSS_GRAPH_BG, layers, NT_LEGEND_NDX)) return NULL;
    g_slist_free(layers); }
  { gr_set_font(); series_reg_on_scale(graph_marks); }
  return &graphtab;
#undef GR_INIT_LAYER
}

void graphtab_free(void) {
  if (grid_pango)  { g_object_unref(grid_pango);  grid_pango = NULL; }
  if (mark_pango)  { g_object_unref(mark_pango);  mark_pango = NULL; }
  if (graph_pango) { g_object_unref(graph_pango); graph_pango = NULL; }
  if (graph_font)  { pango_font_description_free(graph_font); graph_font = NULL; }
  pango_cairo_font_map_set_default(NULL); // a bit less
}

void graphtab_update(void) {
  if (opts.legend) notifier_legend_update();
  if (GTK_IS_WIDGET(graph_graph)) gtk_widget_queue_draw(graph_graph);
}

void graphtab_refresh(void) {
  draw_graph_at = 0;
  if (GTK_IS_WIDGET(graph_grid))  gtk_widget_queue_draw(graph_grid);
  if (GTK_IS_WIDGET(graph_marks)) gtk_widget_queue_draw(graph_marks);
  if (GTK_IS_WIDGET(graph_graph)) gtk_widget_queue_draw(graph_graph);
}

