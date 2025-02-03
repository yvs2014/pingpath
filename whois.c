
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "common.h"

#include "whois.h"
#include "parser.h"

static const char *WHOIS_HOST = "riswhois.ripe.net";
static const int   WHOIS_PORT = 43;

typedef struct wc_elem { // whois cache element
  char *addr;
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

#if WHOIS_EXTRA_DEBUG
#define PR_WHOIS_Q do { LOG("%s: %s %c", LOG_WHOIS_HDR, __func__, 'q'); _pr_whois_qlist(whois_query); } while (0)
#define PR_WHOIS_C do { LOG("%s: %s %c", LOG_WHOIS_HDR, __func__, 'c'); _pr_whois_clist(whois_cache); } while (0)
void _pr_whois_qlist(GSList *qlist) {
  int i = 0;
  for (GSList *p = qlist; p; p = p->next, i++) {
    t_wq_elem *q = p->data; if (!q) continue;
    t_whois *w = &q->data.whois;
    WHOIS_DEBUG("%c[%d]: %s=%s %s=%s %s=%s %s=%s %s=%s", 'q', i,
      ADDR_HDR,      q->data.addr,
      ELEM_AS_HDR,   mnemo(w->elem[WHOIS_AS_NDX]),
      ELEM_CC_HDR,   mnemo(w->elem[WHOIS_CC_NDX]),
      ELEM_RT_HDR,   mnemo(w->elem[WHOIS_RT_NDX]),
      ELEM_DESC_HDR, mnemo(w->elem[WHOIS_DESC_NDX]));
    print_refs(q->refs, LOG_WHOIS_HDR);
  }
}
void _pr_whois_clist(GSList *clist) {
  int i = 0;
  for (GSList *p = clist; p; p = p->next, i++) {
    t_wc_elem *c = p->data; if (!c) continue;
    WHOIS_DEBUG("%c[%d]: %s=%s %s=%s %s=%s %s=%s %s=%s", 'c', i,
      ADDR_HDR,      c->addr,
      ELEM_AS_HDR,   mnemo(c->whois.elem[WHOIS_AS_NDX]),
      ELEM_CC_HDR,   mnemo(c->whois.elem[WHOIS_CC_NDX]),
      ELEM_RT_HDR,   mnemo(c->whois.elem[WHOIS_RT_NDX]),
      ELEM_DESC_HDR, mnemo(c->whois.elem[WHOIS_DESC_NDX]));
  }
}
#else
#define PR_WHOIS_Q NOOP
#define PR_WHOIS_C NOOP
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
  if (elem) { CLR_STR(elem->addr); for (int i = 0; i < WHOIS_NDX_MAX; i++) CLR_STR(elem->whois.elem[i]); }
}

static void wq_free(t_wq_elem *elem) {
  if (elem) {
    wc_free(&elem->data);
    g_slist_free_full(elem->refs, g_free);
    CLR_STR(elem->buff);
    elem->size = 0;
  }
}

static t_wc_elem* wc_find(char* addr) {
  t_wc_elem find = { .addr = addr };
  GSList *found = g_slist_find_custom(whois_cache, &find, wc_cmp);
  return found ? found->data : NULL;
}

static t_wq_elem* wq_find(char* addr) {
  t_wq_elem find = { .data = { .addr = addr }};
  GSList *found = g_slist_find_custom(whois_query, &find, wq_cmp);
  return found ? found->data : NULL;
}

static gboolean whois_elems_not_null(t_whois *whois) {
  if (!whois) return false;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) if (!whois->elem[i]) return false;
  return true;
}

static void whois_query_close(t_wq_elem *elem) {
  if (!elem) return;
  if (G_IS_SOCKET_CONNECTION(elem->conn) && !g_io_stream_is_closed(G_IO_STREAM(elem->conn)))
    g_io_stream_close(G_IO_STREAM(elem->conn), NULL, NULL);
  whois_query = g_slist_remove(whois_query, elem);
  wq_free(elem); g_free(elem);
}

static void whois_copy_elems(t_whois *from, t_whois *to, gboolean *wcached, gboolean *wcached_nl) {
  if (!from || !to) return;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) {
    UPD_NSTR(to->elem[i], from->elem[i], MAXHOSTNAME);
    if (wcached) wcached[i] = false;
    if (wcached_nl) wcached_nl[i] = false;
  }
}

static void whois_query_complete(t_ref *ref, t_wq_elem *elem) {
  if (!ref || !elem) return;
  t_hop *hop = ref->hop;
  if (hop) {
    int ndx = ref->ndx;
    char *orig = hop->host[ndx].addr;
    char *addr = elem->data.addr;
    if (STR_EQ(orig, addr))
      whois_copy_elems(&elem->data.whois, &hop->whois[ndx], hop->wcached, hop->wcached_nl);
    else
      LOG("%s: %s: %s -> %s", LOG_WHOIS_HDR, ORIG_CHNG_HDR, orig, addr);
  }
}

static GSList* list_add_wc(GSList **list, t_wc_elem *wc) {
  if (!list || !wc) return NULL;
  GSList *elem = g_slist_find_custom(*list, wc, wc_cmp);
  if (elem) { g_free(wc); return elem; }
  *list = g_slist_prepend(*list, wc);
  if (g_slist_length(*list) > WHOIS_CACHE_MAX) {
    GSList *last = g_slist_last(*list);
    if (last) {
      t_wc_elem *elem = last->data;
      if (elem) { *list = g_slist_remove(*list, elem); wc_free(elem); g_free(elem); }
      else *list = g_slist_delete_link(*list, last);
    }
  }
  return *list;
}

static void whois_cache_update(char *addr, t_whois *whois) {
  if (!addr || !whois) return;
  t_wc_elem *cache = wc_find(addr);
  if (cache) {
    whois_copy_elems(whois, &cache->whois, NULL, NULL);
    PR_WHOIS_C; return;
  }
  cache = g_malloc0(sizeof(t_wc_elem));
  if (cache) {
    cache->addr = g_strndup(addr, MAXHOSTNAME);
    t_whois *cached = &cache->whois;
    for (int i = 0; i < WHOIS_NDX_MAX; i++) UPD_NSTR(cached->elem[i], whois->elem[i], MAXHOSTNAME);
    if (cache->addr && whois_elems_not_null(cached)) {
      if (list_add_wc(&whois_cache, cache)) {
        LOG("%s: %s=%s %s=%s %s=%s %s=%s %s=%s", WHOIS_CUP_HDR,
          ADDR_HDR,      cache->addr,
          ELEM_AS_HDR,   mnemo(cached->elem[WHOIS_AS_NDX]),
          ELEM_CC_HDR,   mnemo(cached->elem[WHOIS_CC_NDX]),
          ELEM_RT_HDR,   mnemo(cached->elem[WHOIS_RT_NDX]),
          ELEM_DESC_HDR, mnemo(cached->elem[WHOIS_DESC_NDX]));
        PR_WHOIS_C;
        return;
      }
      FAILX(addr, "add cache");
    } else FAILX(addr, "dup host");
  } else FAILX(addr, "alloc host");
  wc_free(cache);
  g_free(cache);
}

static t_wq_elem* whois_query_save(const char *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_wq_elem *elem = g_malloc0(sizeof(t_wq_elem));
  if (!elem) return NULL;
  elem->buff = g_malloc0(NET_BUFF_SIZE); elem->size = 0;
  if (elem->buff) {
    elem->data.addr = g_strndup(addr, MAXHOSTNAME);
    if (elem->data.addr) {
      if (list_add_ref(&elem->refs, hop, ndx)) {
        GSList *added = list_add_nodup(&whois_query, elem, wq_cmp, WHOIS_QUERY_MAX);
        if (added) {
          WHOIS_DEBUG("%s: %s %s=%d %s[%d]=%s", SAVEQ_HDR, addr,
            HOP_HDR, hop->at, ADDR_HDR, ndx, hop->host[ndx].addr);
          PR_WHOIS_Q;
          return added->data;
        }
        FAILX(addr, "add query");
      } else FAILX(addr, "add ref");
    } else FAILX(addr, "dup addr");
  } else FAILX(addr, "alloc buffer");
  wq_free(elem); g_free(elem);
  return NULL;
}

static t_wq_elem* whois_query_fill(char *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  t_wq_elem *found = wq_find(addr);
  if (found) {
    if (!list_add_ref(&found->refs, hop, ndx)) FAILX(addr, "add ref");
    return NULL;
  }
  WHOIS_DEBUG("%s %s: %s=%d %s[%d]=%s", LOOKUP_HDR, addr, HOP_HDR, hop->at + 1,
   ADDR_HDR, ndx, hop->host[ndx].addr);
  return whois_query_save(addr, hop, ndx);
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wq_elem *elem);

static gboolean whois_reset_read(GObject *stream, ssize_t size, t_wq_elem *elem) {
  if (!elem || !elem->buff || !G_IS_INPUT_STREAM(stream)) return false;
  elem->size += size;
  ssize_t left = NET_BUFF_SIZE - elem->size;
  gboolean reset_read = left > 0;
  if (reset_read) { // continue unless EOF
    char *off = elem->buff + elem->size;
    *off = 0;
    g_input_stream_read_async(G_INPUT_STREAM(stream), off, left, G_PRIORITY_DEFAULT, NULL,
      (GAsyncReadyCallback)on_whois_read, elem);
  } else {
    WARN("%s: %d", NOBUFF_ERR, NET_BUFF_SIZE);
    elem->buff[NET_BUFF_SIZE - 1] = 0;
  }
  return reset_read;
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wq_elem *elem) {
  if (!elem || !G_IS_INPUT_STREAM(stream)) return;
  GError *error = NULL;
  ssize_t size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) { ERROR("g_input_stream_read_finish()"); }
  else {
    if (!elem->buff) return; // possible at external exit
    if (size) { if (whois_reset_read(stream, size, elem)) return; }
    else { // EOF (size == 0)
      t_whois *whois = &elem->data.whois;
      parser_whois(elem->buff, &elem->data.whois);
      if (whois->elem[WHOIS_AS_NDX] && !whois->elem[WHOIS_AS_NDX][0])
        WHOIS_DEBUG("%s: %s", UNRES_HDR, elem->data.addr);
      WHOIS_DEBUG("%s: %s=%s %s=%s %s=%s %s=%s", elem->data.addr,
        ELEM_AS_HDR,   mnemo(whois->elem[WHOIS_AS_NDX]),
        ELEM_CC_HDR,   mnemo(whois->elem[WHOIS_CC_NDX]),
        ELEM_RT_HDR,   mnemo(whois->elem[WHOIS_RT_NDX]),
        ELEM_DESC_HDR, mnemo(whois->elem[WHOIS_DESC_NDX]));
      PR_WHOIS_Q;
      whois_cache_update(elem->data.addr, whois);
      g_slist_foreach(elem->refs, (GFunc)whois_query_complete, elem);
      whois_query_close(elem);
      PR_WHOIS_Q;
    }
  }
}

static void on_whois_write_all(GObject *stream, GAsyncResult *result, char *request) {
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
      }
    } else FAILX(elem->input ? "output" : "input", "get stream");
  } else {
    if (error) LOG("%s %s: rc=%d, %s", LOG_WHOIS_HDR, NOCONN_ERR, error->code, error->message);
    else       LOG("%s %s: %s", LOG_WHOIS_HDR, NOCONN_ERR, UNKN_ERR);
  }
  g_error_free(error);
  whois_query_close(elem);
}


// pub
//

void whois_resolv(t_hop *hop, int ndx) {
  if (!hop) return;
  char *addr = hop->host[ndx].addr;
  if (!addr) return;
  PR_WHOIS_C;
  t_wc_elem *cached = wc_find(addr);
  if (cached) { // update with cached data and return
    whois_copy_elems(&cached->whois, &hop->whois[ndx], hop->wcached, hop->wcached_nl);
    WHOIS_DEBUG("%s [%d,%d]: %s -> %s=%s %s=%s %s=%s %s=%s",
      CACHE_HIT_HDR, hop->at, ndx, addr,
      ELEM_AS_HDR,   mnemo(cached->whois.elem[WHOIS_AS_NDX]),
      ELEM_CC_HDR,   mnemo(cached->whois.elem[WHOIS_CC_NDX]),
      ELEM_RT_HDR,   mnemo(cached->whois.elem[WHOIS_RT_NDX]),
      ELEM_DESC_HDR, mnemo(cached->whois.elem[WHOIS_DESC_NDX]));
    return;
  }
  PR_WHOIS_Q;
  t_wq_elem *query = whois_query_fill(addr, hop, ndx);
  if (!query) return;
  GSocketClient *sock = g_socket_client_new();
  if (sock) {
    WHOIS_DEBUG("%s %s:%d %s", SENDQ_HDR, WHOIS_HOST, WHOIS_PORT, addr);
    g_socket_client_connect_to_host_async(sock, WHOIS_HOST, WHOIS_PORT, NULL,
      (GAsyncReadyCallback)on_whois_connect, query);
    g_object_unref(sock);
  } else FAIL("g_socket_client_new()");
}

static void wc_on_free(gpointer data, gpointer user_data G_GNUC_UNUSED) { wc_free(data); }
static void wq_on_close(gpointer data, gpointer user_data G_GNUC_UNUSED) { whois_query_close(data); }

void whois_cache_free(void) {
  PR_WHOIS_C;
  g_slist_foreach(whois_cache, wc_on_free, NULL);
  g_slist_free_full(whois_cache, g_free);
  whois_cache = NULL;
  PR_WHOIS_Q;
  g_slist_foreach(whois_query, wq_on_close, NULL);
  g_slist_free(whois_query);
  whois_query = NULL;
}

