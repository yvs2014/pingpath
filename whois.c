
#include "whois.h"
#include "common.h"
#include "parser.h"
#include "stat.h"

#define WHOIS_HOST          "riswhois.ripe.net"
#define WHOIS_PORT          43
#define WHOIS_REQUEST_FMT   "-m %s\n"

#define WN_CACHE_MAX MAXTTL
#define WN_DATA_MAX  MAXTTL

typedef struct wn_data {  // network stuff
  GSList *refs;           // hops requested whois resolv
  gchar *addr;            // copy of origin addr from request
  char *buff; int size;   // data buffer and its actual size
  GSocketConnection *conn;
  GInputStream *input;
  GOutputStream *output;
} t_wn_data;

typedef struct wn_cache {
  gchar *addr;
  t_whois whois;
} t_wn_cache;

typedef struct wn_data_n_cache {
  t_wn_data *data;
  t_wn_cache *cache;
} t_wn_data_n_cache;

static t_wn_data wn_data[WN_DATA_MAX];
static GSList *wn_cache;
static int wnc_next_ndx;


// aux
//

//static GSList* whois_cache_find_addr(gchar* addr) {
//  t_whoaddr find = { .addr = addr };
//  return g_slist_find_custom(wn_cache, &find, (GCompareFunc)find_whoaddr);
//}

//static GSList* whois_cache_set_addr(const gchar *addr) {
//  int i = wnc_next_ndx++;
//  if (wnc_next_ndx >= WN_CACHE_MAX) wnc_next_ndx = 0;
//  UPD_STR(wn_cache[i].addr, g_strndup(addr, MAXHOSTNAME));
//  for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(wn_cache[i].whois.elem[j], NULL);
//  return &wn_cache[i];
//}

//static GSList* whois_cache_fill(t_wn_data *data, bool okay) {
//  if (!data || !data->addr) return NULL;
//  wn_cache = add_whoaddr(wn_cache, data->addr);
//  t_whoaddr *wa = new_whoaddr(data->addr);
//  GSList *cache = g_slist_find_custom(wn_cache, &find, (GCompareFunc)find_whoaddr);
//  t_wn_cache *cache = whois_cache_find_addr(data->addr);
//  if (!cache) cache = whois_cache_set_addr(data->addr);
//  if (okay) parser_whois(data->buff, NET_BUFF_SIZE, cache->whois.elem);
//  else for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(cache->whois.elem[j], NULL);
//  return cache;
//}

static void whois_data_close(t_wn_data *data) {
  if (!data) return;
  if (G_IS_SOCKET_CONNECTION(data->conn) && !g_io_stream_is_closed(G_IO_STREAM(data->conn)))
    g_io_stream_close(G_IO_STREAM(data->conn), NULL, NULL);
  UPD_STR(data->addr, NULL);
  UPD_STR(data->buff, NULL);
  data->size = 0;
  if (data->refs) g_slist_free_full(g_steal_pointer(&data->refs), g_free);
}

static void whois_copy_elems(gchar* from[], gchar* to[], bool wcached[]) {
  if (!from || !to) return;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) {
    UPD_NSTR(to[i], from[i], MAXHOSTNAME);
    if (wcached) wcached[i] = false;
  }
}

static void whois_data_complete(t_hop_ref *hr, t_wn_data_n_cache *dc) {
  if (!hr || !dc || !dc->data || !dc->cache) return;
  t_hop *hop = hr->hop;
  if (hop) {
    int ndx = hr->ndx;
    gchar *orig = hop->host[ndx].addr;
    gchar *addr = dc->data->addr;
    if (STR_EQ(orig, addr)) {
      gchar **elems = hop->whois[ndx].elem;
      whois_copy_elems(dc->cache->whois.elem, elems, hop->wcached);
      stat_check_whois_max(elems);
    } else LOG("whois(%s) origin is changed: %s", addr, orig);
  }
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wn_data *data);

static bool whois_reset_read(GObject *stream, gssize size, t_wn_data *data) {
  if (!data || !G_IS_INPUT_STREAM(stream)) return false;
  data->size += size;
  gssize left = NET_BUFF_SIZE - data->size;
  bool reset_read = left > 0;
  if (reset_read) { // continue unless EOF
    char *off = data->buff + data->size;
    *off = 0;
    g_input_stream_read_async(G_INPUT_STREAM(stream), off, left, G_PRIORITY_DEFAULT, NULL,
      (GAsyncReadyCallback)on_whois_read, data);
  } else {
    WARN("buffer[%d]: no space left", NET_BUFF_SIZE);
    data->buff[NET_BUFF_SIZE - 1] = 0;
  }
  return reset_read;
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wn_data *data) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  GError *error = NULL;
  gssize size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &error);
  if (size < 0) { ERROR("stream read"); }
  else if (size) { if (whois_reset_read(stream, size, data)) return; }
  else { // EOF (size == 0)
    t_wn_data_n_cache dc = { .data = data, .cache = whois_cache_fill(data, !size) };
    if (dc.data && dc.cache) g_slist_foreach(dc.data->refs, (GFunc)whois_data_complete, &dc);
    whois_data_close(data);
  }
}

static void on_whois_write_all(GObject *stream, GAsyncResult *result, gchar *request) {
  if (!G_IS_OUTPUT_STREAM(stream)) return;
  GError *error = NULL;
  if (!g_output_stream_write_all_finish(G_OUTPUT_STREAM(stream), result, NULL, &error))
    ERROR("stream write");
  g_free(request);
}

static void on_whois_connect(GObject* source, GAsyncResult *result, t_wn_data *data) {
  if (!data) return;
  GError *error = NULL;
  data->conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source), result, &error);
  if (data->conn) {
    data->input = g_io_stream_get_input_stream(G_IO_STREAM(data->conn));
    data->output = g_io_stream_get_output_stream(G_IO_STREAM(data->conn));
    if (data->input && data->output) {
      char *request = g_strdup_printf("-m %s\n", data->addr); // form request
      if (request) {
        g_input_stream_read_async(data->input, data->buff, NET_BUFF_SIZE,
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_read, data);
        g_output_stream_write_all_async(data->output, request, strnlen(request, MAXHOSTNAME),
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_write_all, request);
        return;
      } else WARN("%s: g_strdup_printf() failed", data->addr);
    } else WARN("get %s stream failed", data->input ? "output" : "input");
  } else ERRLOG("whois connection failed");
  whois_data_close(data);
}

static t_wn_data* whois_data_save(const gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WN_DATA_MAX; i++) {
    if (!wn_data[i].addr) { // vacant
      UPD_STR(wn_data[i].addr, g_strndup(addr, MAXHOSTNAME));
      if (!wn_data[i].addr) { WARN("%s: g_strndup() failed", addr); return NULL; }
      wn_data[i].buff = g_malloc0(NET_BUFF_SIZE);
      if (!wn_data[i].buff) {
        UPD_STR(wn_data[i].addr, NULL);
        WARN("%s: g_malloc0() failed", addr);
        return NULL;
      }
      ADD_REF_OR_RET(&wn_data[i].refs);
      return &wn_data[i];
    }
  }
  WARN("%s: no vacant slots", addr);
  return NULL;
}

static t_wn_data* whois_data_add_addr(const gchar *addr, t_hop *hop, int ndx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WN_DATA_MAX; i++) {
    if (STR_EQ(addr, wn_data[i].addr)) {
      ADD_REF_OR_RET(&wn_data[i].refs);
      return NULL;
    }
  }
  return whois_data_save(addr, hop, ndx);
}


// pub
//

void whois_resolv(t_hop *hop, int ndx) {
  if (hop) {
    gchar *addr = hop->host[ndx].addr;
    if (addr) {
      t_wn_cache *cache = whois_cache_find_addr(addr);
      if (cache) // update with cached data and return
        whois_copy_elems(cache->whois.elem, hop->whois[ndx].elem, hop->wcached);
      else {
        t_wn_data *data = whois_data_add_addr(addr, hop, ndx);
        if (data) {
          GSocketClient *sock = g_socket_client_new();
          if (sock) {
            g_socket_client_connect_to_host_async(sock, WHOIS_HOST, WHOIS_PORT, NULL,
              (GAsyncReadyCallback)on_whois_connect, data);
            g_object_unref(sock);
          } else WARN("%s failed", "g_socket_client_new()");
        }
      }
    }
  }
}

void whois_free_cache(void) {
  for (int i = 0; i < WN_CACHE_MAX; i++) {
    UPD_STR(wn_cache[i].addr, NULL);
    for (int j = 0; j < WHOIS_NDX_MAX; j++)
      UPD_STR(wn_cache[i].whois.elem[j], NULL);
  }
  for (int i = 0; i < WN_DATA_MAX; i++)
    whois_data_close(&wn_data[i]);
}

