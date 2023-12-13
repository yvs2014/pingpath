
#include "whois.h"
#include "parser.h"
#include "stat.h"
#include "common.h"

#define WHOIS_HOST          "riswhois.ripe.net"
#define WHOIS_PORT          43
#define WHOIS_REQUEST_FMT   "-m %s\n"
#define WHOIS_NET_BUFF_SIZE 4096 // suppose it's enough (data is usually 200-300 bytes)

#define WHOIS_WARN(fmt, ...) LOG("WARN: %s: " fmt, __func__, __VA_ARGS__)
#define WHOIS_ERR(at) { LOG("WARN: %s: %s: %s", __func__, at, err ? err->message : UNKN_ERR); g_error_free(err); }

#define WN_CACHE_MAX MAXTTL
#define WN_DATA_MAX  MAXTTL
#define WN_REF_MAX   MAXTTL

typedef struct hop_ref { t_hop *hop; int andx; } t_hop_ref;

typedef struct wn_data { // network stuff
  t_hop_ref ref[WN_REF_MAX]; // hops requested whois resolv
  gchar *addr;               // copy of origin addr from request
  char *buff; int size;      // data buffer and its actual size
  GSocketClient *sock;
  GSocketConnection *conn;
  GInputStream *input;
  GOutputStream *output;
} t_wn_data;

typedef struct wn_cache {
  gchar addr[BUFF_SIZE];
  gchar* elem[WHOIS_NDX_MAX];
} t_wn_cache;

static t_wn_data wn_data[WN_DATA_MAX];
static t_wn_cache wn_cache[WN_CACHE_MAX]; // circular list
static int wnc_next_ndx;


// aux
//

static t_wn_cache* addr_in_wnc(const gchar* addr) {
  for (int i = 0; i < WN_CACHE_MAX; i++) if (STR_EQ(addr, wn_cache[i].addr)) return &wn_cache[i];
  return NULL;
}

static t_wn_cache* wnc_set_addr(const gchar *addr) {
  int i = wnc_next_ndx++;
  if (wnc_next_ndx >= WN_CACHE_MAX) wnc_next_ndx = 0; //1; // 0 is reserved for undefined
  g_strlcpy(wn_cache[i].addr, addr, sizeof(wn_cache[i].addr));
  for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(wn_cache[i].elem[j], NULL);
  return &wn_cache[i];
}

static t_wn_cache* fill_wnc(t_wn_data *wnd, bool okay) {
  if (!wnd || !wnd->addr) return NULL;
  t_wn_cache *wnc = addr_in_wnc(wnd->addr);
  if (!wnc) wnc = wnc_set_addr(wnd->addr);
  if (okay) parser_whois(wnd->buff, WHOIS_NET_BUFF_SIZE, wnc->elem);
  else for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(wnc->elem[j], NULL);
  return wnc;
}

static void close_wnd(t_wn_data *wnd) {
  if (!wnd) return;
  if (G_IS_SOCKET_CONNECTION(wnd->conn) && !g_io_stream_is_closed(G_IO_STREAM(wnd->conn)))
    g_io_stream_close(G_IO_STREAM(wnd->conn), NULL, NULL);
  if (wnd->sock) { g_object_unref(wnd->sock); wnd->sock = NULL; }
  UPD_STR(wnd->addr, NULL);
  UPD_STR(wnd->buff, NULL);
  wnd->size = 0;
  for (int i = 0; i < WN_REF_MAX; i++) {
    wnd->ref[i].hop = NULL;
    wnd->ref[i].andx = 0;
  }
}

static void dup_whois_elems(gchar* from[], gchar* to[], bool wcached[]) {
  if (!from || !to) return;
  for (int i = 0; i < WHOIS_NDX_MAX; i++) {
    to[i] = g_strndup(from[i], MAXHOSTNAME);
    if (wcached) wcached[i] = false;
  }
}

static void complete_wnd(t_wn_data *wnd, t_wn_cache *wnc) {
  if (!wnd || !wnc) return;
  for (int i = 0; i < WN_REF_MAX; i++) {
    t_hop *hop = wnd->ref[i].hop;
    if (hop) {
      int andx = wnd->ref[i].andx;
      gchar *orig = hop->host[andx].addr;
      if (STR_EQ(wnd->addr, orig)) {
        dup_whois_elems(wnc->elem, hop->whois[andx].elem, hop->wcached);
        stat_check_whois_max(hop->whois[andx].elem);
      } else LOG("whois(%s) origin is changed: %s", wnd->addr, orig);
    }
  }
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wn_data *wnd);

static bool whois_reset_read(gssize size, t_wn_data *wnd) {
  if (!wnd) return false;
  wnd->size += size;
  gssize left = WHOIS_NET_BUFF_SIZE - wnd->size;
  bool reset_read = left > 0;
  if (reset_read) { // continue unless EOF
    char *off = wnd->buff + wnd->size;
    *off = 0;
    g_input_stream_read_async(wnd->input, off, left, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_read, wnd);
  } else {
    WHOIS_WARN("buffer[%d]: no space left", WHOIS_NET_BUFF_SIZE);
    wnd->buff[WHOIS_NET_BUFF_SIZE - 1] = 0;
  }
  return reset_read;
}

static void on_whois_read(GObject *stream, GAsyncResult *result, t_wn_data *wnd) {
  if (!G_IS_INPUT_STREAM(stream)) return;
  GError *err = NULL;
  gssize size = g_input_stream_read_finish(G_INPUT_STREAM(stream), result, &err);
  if (size < 0) { WHOIS_ERR("stream read"); }
  else if (size) { if (whois_reset_read(size, wnd)) return; }
  complete_wnd(wnd, fill_wnc(wnd, !size)); // EOF (size == 0)
  close_wnd(wnd);
}

static void on_whois_write_all(GObject *stream, GAsyncResult *result, gchar *request) {
  if (!G_IS_OUTPUT_STREAM(stream)) return;
  GError *err = NULL;
  if (!g_output_stream_write_all_finish(G_OUTPUT_STREAM(stream), result, NULL, &err)) WHOIS_ERR("stream write");
  g_free(request);
}

static void on_whois_connect(GObject* src, GAsyncResult *res, t_wn_data *wnd) {
  if (!wnd || !wnd->sock) return;
  GError *err = NULL;
  wnd->conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(src), res, &err);
  if (wnd->conn) {
    wnd->input = g_io_stream_get_input_stream(G_IO_STREAM(wnd->conn));
    wnd->output = g_io_stream_get_output_stream(G_IO_STREAM(wnd->conn));
    if (wnd->input && wnd->output) {
      char *request = g_strdup_printf("-m %s\n", wnd->addr); // form request
      if (request) {
        g_input_stream_read_async(wnd->input, wnd->buff, WHOIS_NET_BUFF_SIZE,
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_read, wnd);
        g_output_stream_write_all_async(wnd->output, request, strnlen(request, MAXHOSTNAME),
          G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_write_all, request);
        return;
      } else WHOIS_WARN("%s: g_strdup_printf() failed", wnd->addr);
    } else WHOIS_WARN("get %s stream failed", wnd->input ? "output" : "input");
  } else WHOIS_ERR("connection failed");
  close_wnd(wnd);
}

static void add_ref_to_wnd(t_wn_data *wnd, t_hop *hop, int andx) {
  if (!wnd || !hop) return;
  int vacant = -1;
  for (int i = 0; i < WN_REF_MAX; i++) {
    if ((wnd->ref[i].hop == hop) && (wnd->ref[i].andx == andx)) {
      WHOIS_WARN("%s is already in list", wnd->addr);
      return;
    }
    if (!wnd->ref[i].hop) vacant = i;
  }
  if (vacant < 0) WHOIS_WARN("%s: no vacant references", wnd->addr);
  else {
    wnd->ref[vacant].hop = hop;
    wnd->ref[vacant].andx = andx;
  }
}

static t_wn_data* save_wnd(const gchar *addr, t_hop *hop, int andx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WN_DATA_MAX; i++) {
    if (!wn_data[i].addr) { // vacant
      UPD_STR(wn_data[i].addr, g_strndup(addr, MAXHOSTNAME));
      if (!wn_data[i].addr) { WHOIS_WARN("%s: g_strndup() failed", addr); return NULL; }
      wn_data[i].buff = g_malloc(WHOIS_NET_BUFF_SIZE);
      if (!wn_data[i].buff) {
        UPD_STR(wn_data[i].addr, NULL);
        WHOIS_WARN("%s: g_malloc() failed", addr);
        return NULL;
      }
      add_ref_to_wnd(&wn_data[i], hop, andx);
      return &wn_data[i];
    }
  }
  WHOIS_WARN("%s: no vacant slots", addr);
  return NULL;
}

static t_wn_data* new_addr_in_wnd(const gchar *addr, t_hop *hop, int andx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WN_DATA_MAX; i++) {
    if (STR_EQ(addr, wn_data[i].addr)) {
      add_ref_to_wnd(&wn_data[i], hop, andx);
      return NULL;
    }
  }
  return save_wnd(addr, hop, andx);
}


// pub
//

void whois_resolv(t_hop *hop, int andx) {
  if (!hop) return;
  gchar *addr = hop->host[andx].addr;
  if (!addr) return;
  t_wn_cache *wnc = addr_in_wnc(addr);
  if (wnc) // update with cached data and return
    dup_whois_elems(wnc->elem, hop->whois[andx].elem, hop->wcached);
  else {
    t_wn_data *wn_data = new_addr_in_wnd(addr, hop, andx);
    if (wn_data) {
      wn_data->sock = g_socket_client_new();
      if (wn_data->sock)
        g_socket_client_connect_to_host_async(wn_data->sock, WHOIS_HOST, WHOIS_PORT,
          NULL, (GAsyncReadyCallback)on_whois_connect, wn_data);
      else WHOIS_WARN("%s failed", "g_socket_client_new()");
    }
  }
}

void whois_free_cache(void) {
  for (int i = 0; i < WN_CACHE_MAX; i++) {
    wn_cache[i].addr[0] = 0;
    for (int j = 0; j < WHOIS_NDX_MAX; j++)
      UPD_STR(wn_cache[i].elem[j], NULL);
  }
  for (int i = 0; i < WN_DATA_MAX; i++)
    close_wnd(&wn_data[i]);
}

