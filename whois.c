
#include "whois.h"
#include "parser.h"
#include "stat.h"

#define WHOIS_HOST          "riswhois.ripe.net"
#define WHOIS_PORT          43
#define WHOIS_REQUEST_FMT   "-m %s\n"

typedef struct wc_elem { // whois cache element
  gchar *addr;
  t_whois whois;
} t_wc_elem;

typedef struct wq_data {  // whois query element
  GSList *refs;           // hops requested whois resolv
  t_wc_elem data;         // copy of origin addr and resulting resolved whois fields
  char *buff; int size;   // data buffer and its actual size
  GSocketConnection *conn;
  GInputStream *input;
  GOutputStream *output;
} t_wq_elem;

static GSList *whois_query;
static GSList *whois_cache;

#ifdef WHOIS_DEBUGGING
#define WHOIS_DEBUG(fmt, ...) LOG("WHOIS: " fmt, __VA_ARGS__)
#else
#define WHOIS_DEBUG(...) {}
#endif

#if WHOIS_DEBUGGING && WHOIS_DEBUGGING > 1
#define PR_WHOIS_Q { LOG("WHOIS: %s: q=%p", __func__, whois_query); _pr_whois_qlist(whois_query); }
#define PR_WHOIS_C { LOG("WHOIS: %s: c=%p", __func__, whois_cache); _pr_whois_clist(whois_cache); }
void _pr_whois_qlist(GSList *qlist) {
  int i = 0;
  for (GSList *p = qlist; p; p = p->next, i++) {
    t_wq_elem *q = p->data; if (!q) continue;
    gchar **w = q->data.whois.elem;
    WHOIS_DEBUG("c[%d]: refs=%p addr=%s as=%s cc=%s rt=%s desc=%s", i, q->refs, q->data.addr, w[WHOIS_AS_NDX],
      w[WHOIS_CC_NDX], w[WHOIS_RT_NDX], w[WHOIS_DESC_NDX]);
    print_refs(q->refs, "WHOIS:  ");
  }
}
void _pr_whois_clist(GSList *clist) {
  int i = 0;
  for (GSList *p = clist; p; p = p->next, i++) {
    t_wc_elem *c = p->data; if (!c) continue;
    WHOIS_DEBUG("c[%d]: addr=%s as=%s cc=%s rt=%s desc=%s", i, c->addr, c->whois.elem[WHOIS_AS_NDX],
      c->whois.elem[WHOIS_CC_NDX], c->whois.elem[WHOIS_RT_NDX], c->whois.elem[WHOIS_DESC_NDX]);
  }
}
#else
#define PR_WHOIS_Q {}
#define PR_WHOIS_C {}
#endif

static int wc_cmp(const void *a, const void *b) {
  if (!a || !b) return -1;
  return g_strcmp0(((t_wc_elem*)a)->addr, ((t_wc_elem*)b)->addr);
}

static int wq_cmp(const void *a, const void *b) {
  if (!a || !b) return -1;
  return g_strcmp0(((t_wq_elem*)a)->data.addr, ((t_wq_elem*)b)->data.addr);
}

static void wc_free(t_wc_elem *elem) {
  if (elem) {
    UPD_STR(elem->addr, NULL);
    for (int i = 0; i < WHOIS_NDX_MAX; i++) UPD_STR(elem->whois.elem[i], NULL);
  }
}

static void wq_free(t_wq_elem *elem) {
  if (elem) {
    wc_free(&elem->data);
    g_slist_free_full(elem->refs, g_free);
    UPD_STR(elem->buff, NULL);
    elem->size = 0;
  }
}

static t_wc_elem* wc_find(gchar* addr) {
  t_wc_elem find = { .addr = addr };
  GSList *found = g_slist_find_custom(whois_cache, &find, wc_cmp);
  return found ? found->data : NULL;
}

static t_wq_elem* wq_find(gchar* addr) {
  t_wq_elem find = { .data = { .addr = addr }};
  GSList *found = g_slist_find_custom(whois_query, &find, wq_cmp);
  return found ? found->data : NULL;
}

static bool whois_elems_not_null(gchar **welems) {
  if (!welems) return false;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) if (!welems[i]) return false;
  return true;
}

static void whois_query_close(t_wq_elem *elem) {
  if (!elem) return;
  if (G_IS_SOCKET_CONNECTION(elem->conn) && !g_io_stream_is_closed(G_IO_STREAM(elem->conn)))
    g_io_stream_close(G_IO_STREAM(elem->conn), NULL, NULL);
  wq_free(elem);
  whois_query = g_slist_remove(whois_query, elem);
}

static void whois_copy_elems(gchar* from[], gchar* to[], bool *wcached) {
  if (!from || !to) return;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) {
    UPD_NSTR(to[i], from[i], MAXHOSTNAME);
    if (wcached) wcached[i] = false;
  }
}

static void whois_query_complete(t_ref *ref, t_wq_elem *elem) {
  if (!ref || !elem) return;
  t_hop *hop = ref->hop;
  if (hop) {
    int ndx = ref->ndx;
    gchar *orig = hop->host[ndx].addr;
    gchar *addr = elem->data.addr;
    if (STR_EQ(orig, addr)) {
      gchar **elems = hop->whois[ndx].elem;
      whois_copy_elems(elem->data.whois.elem, elems, hop->wcached);
      stat_check_whois_max(elems);
    } else LOG("whois(%s) origin is changed: %s", addr, orig);
  }
}

static void whois_cache_update(gchar *addr, gchar* welem[]) {
  if (!addr) return;
  t_wc_elem *cache = wc_find(addr);
  if (cache) {
    whois_copy_elems(welem, cache->whois.elem, NULL);
    PR_WHOIS_C; return;
  }
  cache = g_malloc0(sizeof(t_wc_elem));
  if (cache) {
    cache->addr = g_strndup(addr, MAXHOSTNAME);
    gchar **cwelem = cache->whois.elem;
    for (int i = 0; i < WHOIS_NDX_MAX; i++) UPD_NSTR(cwelem[i], welem[i], MAXHOSTNAME);
    if (cache->addr && whois_elems_not_null(cwelem)) {
      if (list_add_nodup(&whois_cache, cache, wc_cmp, WHOIS_CACHE_MAX)) {
        LOG("whois cache updated with addr=%s as=%s cc=%s rt=%s desc=%s", cache->addr,
          cwelem[WHOIS_AS_NDX], cwelem[WHOIS_CC_NDX], cwelem[WHOIS_RT_NDX], cwelem[WHOIS_DESC_NDX]);
        PR_WHOIS_C; return;
      } else WARN("%s: add cache failed", addr);
    } else WARN("%s: dup host failed", addr);
  } else WARN("%s: alloc host failed", addr);
  wc_free(cache);
  g_free(cache);
}

static t_wq_elem* whois_query_save(const gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_wq_elem *elem = g_malloc0(sizeof(t_wq_elem));
  if (!elem) return NULL;
  elem->buff = g_malloc0(NET_BUFF_SIZE); elem->size = 0;
  if (elem->buff) {
    elem->data.addr = g_strndup(addr, MAXHOSTNAME);
    if (elem->data.addr) {
      if (list_add_nodup(&elem->refs, ref_new(hop, ndx), (GCompareFunc)ref_cmp, REF_MAX)) {
        GSList *added = list_add_nodup(&whois_query, elem, wq_cmp, WHOIS_QUERY_MAX);
        if (added) {
          WHOIS_DEBUG("%s(%s) hop[%d] host[%d]=%s", __func__, addr, hop->at, ndx, hop->host[ndx].addr);
          PR_WHOIS_Q; return added->data;
        } else WARN("%s: add query failed", addr);
      } else WARN("%s: add ref failed", addr);
    } else WARN("%s: dup addr failed", addr);
  } else WARN("%s: alloc buffer failed", addr);
  wq_free(elem);
  g_free(elem);
  return NULL;
}

static t_wq_elem* whois_query_fill(gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_wq_elem *found = wq_find(addr);
  if (found) { ADD_REF_OR_RET(&found->refs); return NULL; }
  WHOIS_DEBUG("%s(%s) hop[%d] host[%d]=%s", __func__, addr, hop->at, ndx, hop->host[ndx].addr);
  return whois_query_save(addr, hop, ndx);
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wq_elem *elem);

static bool whois_reset_read(GObject *stream, gssize size, t_wq_elem *elem) {
  if (!elem || !G_IS_INPUT_STREAM(stream)) return false;
  elem->size += size;
  gssize left = NET_BUFF_SIZE - elem->size;
  bool reset_read = left > 0;
  if (reset_read) { // continue unless EOF
    char *off = elem->buff + elem->size;
    *off = 0;
    g_input_stream_read_async(G_INPUT_STREAM(stream), off, left, G_PRIORITY_DEFAULT, NULL,
      (GAsyncReadyCallback)on_whois_read, elem);
  } else {
    WARN("buffer[%d]: no space left", NET_BUFF_SIZE);
    elem->buff[NET_BUFF_SIZE - 1] = 0;
  }
  return reset_read;
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wq_elem *elem) {
  if (!elem || !G_IS_INPUT_STREAM(stream)) return;
  GError *error = NULL;
  gssize size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) { ERROR("g_input_stream_read_finish()"); }
  else if (size) { if (whois_reset_read(stream, size, elem)) return; }
  else { // EOF (size == 0)
    gchar **welem = elem->data.whois.elem;
    parser_whois(elem->buff, elem->size, welem);
    if (welem[WHOIS_AS_NDX] && !welem[WHOIS_AS_NDX][0]) WHOIS_DEBUG("%s unresolved", elem->data.addr);
    WHOIS_DEBUG("%s(%s) as=%s cc=%s rt=%s desc=%s", __func__, elem->data.addr,
      welem[WHOIS_AS_NDX], welem[WHOIS_CC_NDX], welem[WHOIS_RT_NDX], welem[WHOIS_DESC_NDX]);
    PR_WHOIS_Q;
    whois_cache_update(elem->data.addr, welem);
    g_slist_foreach(elem->refs, (GFunc)whois_query_complete, elem);
    whois_query_close(elem);
    PR_WHOIS_Q;
  }
}

static void on_whois_write_all(GObject *stream, GAsyncResult *result, gchar *request) {
  if (!G_IS_OUTPUT_STREAM(stream)) return;
  GError *error = NULL;
  if (!g_output_stream_write_all_finish(G_OUTPUT_STREAM(stream), result, NULL, &error))
    ERROR("g_output_stream_write_all_finish()");
  g_free(request);
}

static void on_whois_connect(GObject* source, GAsyncResult *result, t_wq_elem *elem) {
  if (!elem) return;
  GError *error = NULL;
  elem->conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source), result, &error);
  if (elem->conn) {
    elem->input = g_io_stream_get_input_stream(G_IO_STREAM(elem->conn));
    elem->output = g_io_stream_get_output_stream(G_IO_STREAM(elem->conn));
    if (elem->input && elem->output) {
      char *request = g_strdup_printf("-m %s\n", elem->data.addr); // form request
      if (request) {
        g_input_stream_read_async(elem->input, elem->buff, NET_BUFF_SIZE,
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_read, elem);
        g_output_stream_write_all_async(elem->output, request, strnlen(request, MAXHOSTNAME),
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_write_all, request);
        return;
      } else WARN("%s: g_strdup_printf() failed", elem->data.addr);
    } else WARN("get %s stream failed", elem->input ? "output" : "input");
  } else ERRLOG("whois connection failed");
  whois_query_close(elem);
}


// pub
//

void whois_resolv(t_hop *hop, int ndx) {
  if (!hop) return;
  gchar *addr = hop->host[ndx].addr;
  if (!addr) return;
  PR_WHOIS_C;
  t_wc_elem *cached = wc_find(addr);
  if (cached) { // update with cached data and return
    whois_copy_elems(cached->whois.elem, hop->whois[ndx].elem, hop->wcached);
    WHOIS_DEBUG("cache hit[%d,%d]: addr=%s -> as=%s cc=%s rt=%s desc=%s", hop->at, ndx, addr,
      cached->whois.elem[WHOIS_AS_NDX], cached->whois.elem[WHOIS_CC_NDX],
      cached->whois.elem[WHOIS_RT_NDX], cached->whois.elem[WHOIS_DESC_NDX]);
    return;
  }
  PR_WHOIS_Q;
  t_wq_elem *query = whois_query_fill(addr, hop, ndx);
  if (!query) return;
  GSocketClient *sock = g_socket_client_new();
  if (sock) {
    WHOIS_DEBUG("connect with query=%s", addr);
    g_socket_client_connect_to_host_async(sock, WHOIS_HOST, WHOIS_PORT, NULL,
      (GAsyncReadyCallback)on_whois_connect, query);
    g_object_unref(sock);
  } else WARN_("g_socket_client_new() failed");
}

void whois_cache_free(void) {
  PR_WHOIS_C;
  g_slist_foreach(whois_cache, (GFunc)wc_free, NULL);
  g_slist_free(whois_cache);
  whois_cache = NULL;
  PR_WHOIS_Q;
  g_slist_foreach(whois_query, (GFunc)whois_query_close, NULL);
  g_slist_free(whois_query);
  whois_query = NULL;
}

