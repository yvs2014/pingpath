
#include "whois.h"
#include "parser.h"
#include "stat.h"
#include "common.h"

#define WHOIS_HOST "riswhois.ripe.net"
#define WHOIS_PORT 43
#define WNC_MAX MAXTTL
#define WND_MAX MAXTTL
#define WND_REF_MAX MAXTTL

typedef struct hop_ref { t_hop *hop; int andx; } t_hop_ref;

typedef struct wn_data { // network stuff
  t_hop_ref ref[WND_REF_MAX];
  gchar *addr; // copy of origin addr from request
  GSocketClient *sock;
  GSocketConnection *conn;
  GInputStream *input;
  char buff[BUFF_SIZE * 4]; // suppose it's enough
  int offset; // in buffer
} t_wn_data;

typedef struct wn_cache {
  gchar addr[BUFF_SIZE];
  gchar* elem[WHOIS_NDX_MAX];
} t_wn_cache;

static t_wn_data wn_data[WND_MAX];
static t_wn_cache wn_cache[WNC_MAX]; // circular list
static int wnc_next_ndx;


// aux
//

static t_wn_cache* addr_in_wnc(const gchar* addr) {
  for (int i = 0; i < WNC_MAX; i++) if (STR_EQ(addr, wn_cache[i].addr)) return &wn_cache[i];
  return NULL;
}

static t_wn_cache* wnc_set_addr(const gchar *addr) {
  int i = wnc_next_ndx++;
  if (wnc_next_ndx >= WNC_MAX) wnc_next_ndx = 0; //1; // 0 is reserved for undefined
  g_strlcpy(wn_cache[i].addr, addr, sizeof(wn_cache[i].addr));
  for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(wn_cache[i].elem[j], NULL);
  return &wn_cache[i];
}

static t_wn_cache* fill_wnc(t_wn_data *wnd, bool okay) {
  if (!wnd || !wnd->addr) return NULL;
  t_wn_cache *wnc = addr_in_wnc(wnd->addr);
  if (!wnc) wnc = wnc_set_addr(wnd->addr);
  if (okay) parser_whois(wnd->buff, sizeof(wnd->buff), wnc->elem);
  else for (int j = 0; j < WHOIS_NDX_MAX; j++) UPD_STR(wnc->elem[j], NULL);
  return wnc;
}

static void close_wnd(t_wn_data *wnd) {
  if (!wnd) return;
  g_io_stream_close(G_IO_STREAM(wnd->conn), NULL, NULL);
  g_object_unref(wnd->sock); wnd->sock = NULL;
  g_free(wnd->addr); wnd->addr = NULL;
  for (int i = 0; i < WND_REF_MAX; i++) {
    wnd->ref[i].hop = NULL;
    wnd->ref[i].andx = 0;
  }
  wnd->buff[0] = 0;
  wnd->offset = 0;
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
  for (int i = 0; i < WND_REF_MAX; i++) {
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

static void finish_wnd(t_wn_data *wnd, bool okay) {
  complete_wnd(wnd, fill_wnc(wnd, okay));
  close_wnd(wnd);
}

static void on_whois_data(GObject *stream, GAsyncResult *re, t_wn_data *wnd) {
  if (!wnd || !G_IS_INPUT_STREAM(stream)) return;
  bool okay = false;
  GError *err = NULL;
  int sz = g_input_stream_read_finish(G_INPUT_STREAM(stream), re, &err);
  if ((sz < 0) || err) { // error
    WARN("whois stream read: %s", err ? err->message : "");
  } else if (sz) { // data
    wnd->offset += sz;
    if (wnd->offset < sizeof(wnd->buff)) { // continue unless EOF
      char *off = wnd->buff + wnd->offset;
      *off = 0;
      g_input_stream_read_async(wnd->input, off, sizeof(wnd->buff) - wnd->offset,
        G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_data, wnd);
      return;
    } else LOG("whois-net(%s): buffer[%ld]: no space left in buffer", wnd->addr, sizeof(wnd->buff));
  } else { // EOF (sz == 0)
    okay = true;
  }
  finish_wnd(wnd, okay);
}

static void on_whois_connect(GObject* src, GAsyncResult *res, t_wn_data *wnd) {
  if (!wnd || !wnd->sock) return;
//  if (!wnd->buff) wnd->buff = g_string_sized_new(BUFF_SIZE);
//  if (!wnd->buff) { WARN("%s failed", "g_string_sized_new()"); return; }
  GError *err = NULL;
  wnd->conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(src), res, &err);
  if (!wnd->conn) {
    LOG("whois connection ERROR: %s", err->message);
    g_error_free(err);
    close_wnd(wnd);
    return;
  }
  wnd->input = g_io_stream_get_input_stream(G_IO_STREAM(wnd->conn));
  g_input_stream_read_async(wnd->input, wnd->buff, sizeof(wnd->buff),
    G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)on_whois_data, wnd);
  GOutputStream *output = g_io_stream_get_output_stream(G_IO_STREAM(wnd->conn));
  // write request
  static char req[BUFF_SIZE];
  g_snprintf(req, sizeof(req), "-m %s\n", wnd->addr);
  err = NULL;
  g_output_stream_write(output, req, strnlen(req, sizeof(req)), NULL, &err);
  if (err) {
    LOG("WARN: whois write: %s", err->message);
    finish_wnd(wnd, false);
    return;
  }
}

static void add_ref_to_wnd(t_wn_data *wnd, t_hop *hop, int andx) {
  if (!wnd || !hop) return;
  int vacant = -1;
  for (int i = 0; i < WND_REF_MAX; i++) {
    if ((wnd->ref[i].hop == hop) && (wnd->ref[i].andx == andx)) {
      LOG("WARN: whois-net(%s): it's already in list", wnd->addr);
      return;
    }
    if (!wnd->ref[i].hop) vacant = i;
  }
  if (vacant < 0) LOG("WARN: whois-net(%s): no vacant references", wnd->addr);
  else {
    wnd->ref[vacant].hop = hop;
    wnd->ref[vacant].andx = andx;
  }
}

static t_wn_data* save_wnd(const gchar *addr, t_hop *hop, int andx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WND_MAX; i++) {
    if (!wn_data[i].addr) { // vacant
      wn_data[i].addr = g_strndup(addr, MAXHOSTNAME);
      add_ref_to_wnd(&wn_data[i], hop, andx);
      return &wn_data[i];
    }
  }
  LOG("WARN: whois-net(%s): no vacant slots", addr);
  return NULL;
}

static t_wn_data* new_addr_in_wnd(const gchar *addr, t_hop *hop, int andx) {
  if (!hop || !addr) return NULL;
  for (int i = 0; i < WND_MAX; i++) {
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
  if (wnc) { // update with cached data and return
    dup_whois_elems(wnc->elem, hop->whois[andx].elem, hop->wcached);
    return;
  }
  t_wn_data *wn_data = new_addr_in_wnd(addr, hop, andx);
  if (!wn_data) return;
  wn_data->sock = g_socket_client_new();
  GError *err = NULL;
  if (!wn_data->sock) {
    WARN("whois new socket: %s", err ? err->message : "");
    g_error_free(err);
    return;
  }
  g_socket_client_connect_to_host_async(wn_data->sock, WHOIS_HOST, WHOIS_PORT,
    NULL, (GAsyncReadyCallback)on_whois_connect, wn_data);
}

