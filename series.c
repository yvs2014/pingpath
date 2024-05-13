
#include <series.h>
#include <pinger.h>
#include <stat.h>

t_series_list stat_series[MAXTTL];
const int series_count = MAXTTL;
int series_datamax = PP_RTT0;

static t_series_list stat_series_kept[MAXTTL];
#define KEPT_SERIES(i) stat_series_kept[i].list
#define KEPT_SERIES_LEN(i) stat_series_kept[i].len

#define CURR_SERIES(i) series[i].list
#define CURR_SERIES_LEN(i) series[i].len
#define CURR_SERIES_LAST_SEQ(i) ((CURR_SERIES_LEN(i) && CURR_SERIES(i) && CURR_SERIES(i)->data) ? \
  ((t_rseq*)CURR_SERIES(i)->data)->seq : 0)

static gboolean grm_lock;
static int series_no = MIN_SERIES_RANGE + 1;
GSList *on_scale_list;

//

static void series_scale(int max) {
  series_datamax = max * GRAPH_DATA_GAP;
  for (GSList *item = on_scale_list; item; item = item->next) {
    GtkWidget *w = item->data;
    if (!GTK_IS_WIDGET(w)) continue;
    gtk_widget_queue_draw(w);
    if (!gtk_widget_get_visible(w)) gtk_widget_set_visible(w, true);
  }
}

static int max_in_series(GSList* list) {
  int max = PP_RTT0;
  for (int i = 0; i < series_count; i++)
    for (GSList *item = list; item; item = item->next) {
      if (!item->data) continue;
      int rtt = ((t_rseq*)item->data)->rtt;
      if (rtt > max) max = rtt;
    }
  return max;
}

static void series_save_rseq(int i, t_rseq *data) {
  if (grm_lock || !data) return;
  t_rseq *copy = g_memdup2(data, sizeof(*data));
  if (!copy) return; // highly unlikely
  t_series_list* series = pinger_state.pause ? stat_series_kept : stat_series;
  CURR_SERIES(i) = g_slist_prepend(CURR_SERIES(i), copy); CURR_SERIES_LEN(i)++;
  if (data->rtt > series_datamax) series_scale(data->rtt); // up-scale
  while (CURR_SERIES(i) && (CURR_SERIES_LEN(i) > series_no)) {
    GSList *last = g_slist_last(CURR_SERIES(i));
    if (!last) break;
    int rtt = last->data ? ((t_rseq*)last->data)->rtt : -1;
    if (last->data) { g_free(last->data); last->data = NULL; }
    CURR_SERIES(i) = g_slist_delete_link(CURR_SERIES(i), last); CURR_SERIES_LEN(i)--;
    if ((rtt * GRAPH_DATA_GAP) >= series_datamax) {
      int max = max_in_series(CURR_SERIES(i));
      if (((max * GRAPH_DATA_GAP) < series_datamax) && (max > PP_RTT0)) series_scale(max); // down-scale
    }
  }
}

static void* stat_dup_series(const void *src, void *data) { return g_memdup2(src, sizeof(t_rseq)); }


// pub

void series_update(void) {
  if (grm_lock) return;
  t_series_list* series = pinger_state.pause ? stat_series_kept : stat_series;
  t_rseq skip = { .rtt = -1, .seq = 0 };
  for (int i = 0; i < series_count; i++) {
    if ((opts.min <= i) && (i < tgtat)) {
      int seq = CURR_SERIES_LAST_SEQ(i) + 1;
      t_rseq data = { .seq = seq };
      stat_rseq(i, &data);
      if (data.seq > 0) series_save_rseq(i, &data);
      else /* sync seq */ if (data.seq < 0) { // rarely
        data.seq = -data.seq;
        if (data.seq < seq) DEBUG("sync back: req#%d got#%d", seq, data.seq)
        else { // unlikely, just in case
          series_save_rseq(i, &data);
          DEBUG("sync forward: req#%d got#%d", seq, data.seq);
        }
      }
    } else series_save_rseq(i, &skip);
  }
}

void series_free(gboolean unreg) {
  series_datamax = PP_RTT0;
  for (int i = 0; i < series_count; i++) {
    SERIES_LEN(i) = 0; if (SERIES(i)) { g_slist_free_full(SERIES(i), g_free); SERIES(i) = NULL; }
    KEPT_SERIES_LEN(i) = 0; if (KEPT_SERIES(i)) { g_slist_free_full(KEPT_SERIES(i), g_free); KEPT_SERIES(i) = NULL; }
  }
  if (unreg && on_scale_list) { g_slist_free(on_scale_list); on_scale_list = NULL; }
}

void series_lock(void) {
  grm_lock = true;
  for (int i = 0; i < series_count; i++) { // keep current data
    KEPT_SERIES(i) = SERIES(i) ? g_slist_copy_deep(SERIES(i), stat_dup_series, NULL) : NULL;
    KEPT_SERIES_LEN(i) = g_slist_length(KEPT_SERIES(i));
  }
  grm_lock = false;
}

void series_unlock(void) {
  grm_lock = true;
  for (int i = 0; i < series_count; i++) { // clean current data, copy kept references
    if (SERIES(i)) g_slist_free_full(SERIES(i), g_free);
    SERIES(i) = KEPT_SERIES(i); SERIES_LEN(i) = KEPT_SERIES_LEN(i);
    KEPT_SERIES_LEN(i) = 0; KEPT_SERIES(i) = NULL;
  }
  grm_lock = false;
}

inline void series_min(int no) { if (no > series_no) series_no = no; }

void series_reg_on_scale(GtkWidget *w) {
  if (GTK_IS_WIDGET(w)) on_scale_list = g_slist_append(on_scale_list, w);
}

