
#include "graph.h"
#include "pinger.h"
#include "stat.h"
#include "ui/style.h"

#define X_AXIS "time"
#define Y_AXIS "delay [msec]"

#define GR_FONT "monospace"
#define GRF_RATIO 0.28

#define GR_RTT0 5000 // 5msec
#define GR_RTT_SCALE 1.1

#define GR_LEFT   60
#define GR_TOP    50
#define GR_RIGHT  40
#define GR_BOTTOM 40

#define CELL_SIZE 50
#define TICK_SIZE 6
#define DASH_BOLD 0.7

typedef struct gr_measures {
  int x, y, w, h, N, M;
} t_gr_measures;

typedef struct gr_list {
  GSList* list; int max; // to not calculate length of list
} t_gr_list;

static t_tab graphtab = { .self = &graphtab, .name = "graph-tab",
  .ico = GRAPH_TAB_ICON, .tag = GRAPH_TAB_TAG, .tip = GRAPH_TAB_TIP,
//  .desc = { [POP_MENU_NDX_COPY] = { .name = "win.graph_menu_copy" }, [POP_MENU_NDX_SALL] = { .name = "win.graph_menu_sall" }},
//  .act = { [POP_MENU_NDX_COPY] = { .activate = cb_on_copy_l1 },  [POP_MENU_NDX_SALL] = { .activate = cb_on_sall }},
};

static t_gr_list gr_series[MAXTTL];
#define GRLS (gr_series[i].list)
#define GRMX (gr_series[i].max)
static int gr_max_elems = 10;
static int gr_max_rtt = GR_RTT0;

static GtkWidget *gr_grid, *gr_marks, *gr_graph;
static PangoFontDescription *gr_font;

static t_gr_measures grm = { .x = GR_LEFT, .y = GR_TOP };


static void gr_scale_max(int max) {
  gr_max_rtt = max * GR_RTT_SCALE;
  gtk_widget_queue_draw(gr_marks);
  gtk_widget_set_visible(gr_marks, true);
}

static int gr_max_in_series(void) {
  int max = GR_RTT0;
  for (int i = 0; i < G_N_ELEMENTS(gr_series); i++) {
    for (GSList *list = GRLS; list; list = list->next) {
      if (!list->data) continue;
      int rtt = ((t_stat_data*)list->data)->rtt;
      if (rtt > max) max = rtt;
    }
  }
  return max;
}

static void gr_save(int i, t_stat_data data) {
  t_stat_data *sd = g_memdup2(&data, sizeof(data));
  GRLS = g_slist_prepend(GRLS, sd); GRMX++;
  if (sd && (sd->rtt > gr_max_rtt)) gr_scale_max(sd->rtt); // up-scale
  while (GRLS && (GRMX > gr_max_elems)) {
    GSList *last = g_slist_last(GRLS);
    if (!last) break;
    int rtt = last->data ? ((t_stat_data*)last->data)->rtt : 0;
    GRLS = g_slist_delete_link(GRLS, last); GRMX--;
    if ((rtt * GR_RTT_SCALE) >= gr_max_rtt) {
      int max = gr_max_in_series();
      if ((max * GR_RTT_SCALE) < gr_max_rtt) gr_scale_max(max); // down-scale
    }
  }
}

static void gr_set_font(void) {
  gr_font = pango_font_description_new();
  if (gr_font) {
    pango_font_description_set_family(gr_font, GR_FONT);
    pango_font_description_set_absolute_size(gr_font, grm.y * GRF_RATIO * PANGO_SCALE);
  } else WARN_("Cannot allocate pango font");
}

static void gr_draw_grid(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static PangoLayout *grid_pango;
  static const double dash_size[] = {1};
  if (pinger_state.pause || !area) return;
  int w0 = w - (grm.x + grm.y), h0 = h - (GR_TOP + GR_BOTTOM);
  grm.N = w0 / CELL_SIZE, grm.M = h0 / CELL_SIZE;
  grm.w = grm.N * CELL_SIZE, grm.h = grm.M * CELL_SIZE;
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
  for (int i = 0, x = grm.x + CELL_SIZE; i < grm.N; i++, x += CELL_SIZE) {
    cairo_move_to(cr, x, grm.y + grm.h);
    cairo_rel_line_to(cr, 0, TICK_SIZE);
  }
  for (int j = 0, y = grm.y; j < grm.M; j++, y += CELL_SIZE) {
    cairo_move_to(cr, grm.x, y);
    cairo_rel_line_to(cr, -TICK_SIZE, 0);
  }
  cairo_stroke(cr);
  // axis names
  if (!grid_pango) grid_pango = pango_cairo_create_layout(cr);
  if (grid_pango) {
    if (gr_font) pango_layout_set_font_description(grid_pango, gr_font);
    double fs = grm.y * GRF_RATIO, ts = TICK_SIZE * 2;
    cairo_move_to(cr, grm.x + grm.w + ts, grm.y + grm.h - fs / 2);
    pango_layout_set_text(grid_pango, X_AXIS, -1);
    pango_cairo_show_layout(cr, grid_pango);
    cairo_move_to(cr, grm.x / 4., grm.y - (ts + 1.5 * fs));
    pango_layout_set_text(grid_pango, Y_AXIS, -1);
    pango_cairo_show_layout(cr, grid_pango);
  }
}

static void gr_draw_marks(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  static PangoLayout *mark_pango;
  if (pinger_state.pause || !area || !gr_font || (grm.y <= 0)) return;
  if (!mark_pango) mark_pango = pango_cairo_create_layout(cr);
  if (mark_pango) {
    if (gr_font) pango_layout_set_font_description(mark_pango, gr_font);
    pango_layout_set_width(mark_pango, (grm.x - 2 * TICK_SIZE) * PANGO_SCALE);
    pango_layout_set_alignment(mark_pango, PANGO_ALIGN_RIGHT);
    double fs = grm.y * GRF_RATIO, dy = gr_max_rtt / 1000. / grm.M;
    for (int j = grm.M, y = grm.y - fs * 2 / 3; j >= 0; j--, y += CELL_SIZE) {
      gchar buff[16];
      g_snprintf(buff, sizeof(buff), "%.1f", dy * j);
      cairo_move_to(cr, 0, y);
      pango_layout_set_text(mark_pango, buff, -1);
      pango_cairo_show_layout(cr, mark_pango);
    }
  }
}

static void gr_draw_graph(GtkDrawingArea *area, cairo_t* cr, int w, int h, gpointer unused) {
  if (pinger_state.pause || !area) return;
}

static void graphtab_data_update(void) {
  for (int i = opts.min; i < opts.lim; i++) gr_save(i, stat_data_at(i));
}


// pub
//

t_tab* graphtab_init(GtkWidget* win) {
  graphtab.lab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.lab), NULL);
  graphtab.dyn = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.dyn), NULL);
  // create layers
  gr_grid = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(gr_grid), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gr_grid), gr_draw_grid, NULL, NULL);
  gr_marks = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(gr_marks), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gr_marks), gr_draw_marks, NULL, NULL);
  gr_graph = gtk_drawing_area_new();
  g_return_val_if_fail(GTK_IS_DRAWING_AREA(gr_graph), NULL);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gr_graph), gr_draw_graph, NULL, NULL);
  // merge layers
  GtkWidget *over = gtk_overlay_new();
  g_return_val_if_fail(GTK_IS_OVERLAY(over), NULL);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), gr_grid);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), gr_marks);
  gtk_overlay_add_overlay(GTK_OVERLAY(over), gr_graph);
  gtk_overlay_set_child(GTK_OVERLAY(over), graphtab.dyn);
  // wrap in scroll (not necessary yet)
  graphtab.tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_return_val_if_fail(GTK_IS_BOX(graphtab.tab), NULL);
  GtkWidget *scroll = gtk_scrolled_window_new();
  g_return_val_if_fail(GTK_IS_SCROLLED_WINDOW(scroll), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), over);
  // put into tab
  gtk_widget_set_vexpand(GTK_WIDGET(scroll), true);
  gtk_box_append(GTK_BOX(graphtab.tab), scroll);
  gr_set_font();
  return &graphtab;
}

void graphtab_clear(void) {
  for (int i = 0; i < G_N_ELEMENTS(gr_series); i++) {
    if (GRLS) { g_slist_free(GRLS); GRLS = NULL; }
    GRMX = 0;
  }
  gr_max_rtt = GR_RTT0;
  if (GTK_IS_WIDGET(gr_marks)) gtk_widget_queue_draw(gr_marks);
}

inline void graphtab_update(void) {
  graphtab_data_update();
  gtk_widget_queue_draw(gr_graph);
}

