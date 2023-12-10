
#include "stat.h"
#include "pinger.h"
#include "tabs/ping.h"
#include "common.h"

#define MAX_ADDRS 10

#define STR_EQ(a, b) (!g_strcmp0(a, b))
#define STR_NEQ(a, b) (g_strcmp0(a, b))

// stat formats
#define UNKN    "???"
#define INT_FMT "%d"
#define DBL_FMT "%.*f"
#define DBL_SFX DBL_FMT "%s"
#define ELEM_LEN 5
#define NUM_BUFF_SZ 16

enum { NONE = 0, RX = 1, TX = 2, RXTX = 3 };

typedef struct hop {
  t_host host[MAX_ADDRS]; int pos; bool cached;
  t_whois whois;
  int sent, recv, last, prev, best, wrst;
  double loss, avrg, jttr;
  t_tseq mark;
  bool reach;
  bool tout; // at last update
  gchar* info;
} t_hop;

int hops_no = MAXTTL;
static t_hop hops[MAXTTL];
static t_host_max host_max;
static t_whois_max whois_max;
static int visibles;

#define ELEM_HOST_HDR "Host"
#define ELEM_AS_HDR   "AS"
#define ELEM_CC_HDR   "Country"
#define ELEM_DESC_HDR "Description"
#define ELEM_RT_HDR   "Route"
#define ELEM_LOSS_HDR "Loss"
#define ELEM_SENT_HDR "Sent"
#define ELEM_RECV_HDR "Recv"
#define ELEM_LAST_HDR "Last"
#define ELEM_BEST_HDR "Best"
#define ELEM_WRST_HDR "Wrst"
#define ELEM_AVRG_HDR "Avrg"
#define ELEM_JTTR_HDR "Jttr"

t_stat_elem statelem[ELEM_MAX] = { // TODO: editable from option menu
  [ELEM_NO]   = { .enable = true, .name = "" },
  [ELEM_HOST] = { .enable = true, .name = ELEM_HOST_HDR },
  [ELEM_AS]   = { .enable = true, .name = ELEM_AS_HDR },
  [ELEM_CC]   = { .enable = true, .name = ELEM_CC_HDR },
  [ELEM_DESC] = { .enable = true, .name = ELEM_DESC_HDR },
  [ELEM_RT]   = { .enable = true, .name = ELEM_RT_HDR },
  [ELEM_FILL] = { .enable = true, .name = "" },
  [ELEM_LOSS] = { .enable = true, .name = ELEM_LOSS_HDR },
  [ELEM_SENT] = { .enable = true, .name = ELEM_SENT_HDR },
  [ELEM_RECV] = { .name = ELEM_RECV_HDR },
  [ELEM_LAST] = { .enable = true, .name = ELEM_LAST_HDR },
  [ELEM_BEST] = { .enable = true, .name = ELEM_BEST_HDR },
  [ELEM_WRST] = { .enable = true, .name = ELEM_WRST_HDR },
  [ELEM_AVRG] = { .enable = true, .name = ELEM_AVRG_HDR },
  [ELEM_JTTR] = { .enable = true, .name = ELEM_JTTR_HDR },
};


// aux

static inline void chk_addr_width(int l) {
  if (host_max.addr < l) {
    host_max.addr = l;
    if (!opts.dns) pingtab_update_width(l, ELEM_HOST);
  }
}

static inline void chk_name_width(int l) {
  if (host_max.name < l) {
    host_max.name = l;
    if (opts.dns) pingtab_update_width(l, ELEM_HOST);
  }
}

static void update_hmax(const gchar* addr, const gchar *name) {
  if (addr) {
    int l = g_utf8_strlen(addr, MAXHOSTNAME);
    chk_addr_width(l);
    chk_name_width(name ? g_utf8_strlen(name, MAXHOSTNAME) : l);
  } else if (name)
    chk_name_width(g_utf8_strlen(name, MAXHOSTNAME));
}

static void update_addrname(int at, t_host *b) {
  // addr is mandatory, name not
  bool done;
  int vacant = -1;
  for (int i = 0; i < MAX_ADDRS; i++) {
    t_host *a = &(hops[at].host[i]);
    if ((vacant < 0) && !(a->addr)) vacant = i;
    done = STR_EQ(a->addr, b->addr);
    if (done) {
      hops[at].pos = i;
      if (STR_NEQ(a->name, b->name)) { // add|update NAME
        UPD_STR(a->name, b->name);
        update_hmax(NULL, b->name);
        hops[at].cached = false;
        LOG("set hostname[%d]: %s", at, b->name);
      }
      break;
    }
  }
  if (!done) { // add addrname
    if (vacant < 0) { LOG("no free slots for hop#%d address(%s)", at, b->addr); }
    else {
      hops[at].pos = vacant;
      t_host *a = &(hops[at].host[vacant]);
      UPD_STR(a->addr, b->addr);
      UPD_STR(a->name, b->name);
      update_hmax(b->addr, b->name);
      hops[at].cached = false;
      LOG("set addrname[%d]: %s, %s", at, b->addr, b->name);
    }
  }
  // cleanup dups
  g_free(b->addr);
  g_free(b->name);
}

static void stop_pings_behind(int from, const gchar *reason) {
  for (int i = from; i < MAXTTL; i++) pinger_stop_nth(i, reason);
}

static void set_hops_no(int no, const char *info) {
  hops_no = no;
  stop_pings_behind(no, info);
  pingtab_vis_rows(no);
}

static void uniq_unreach(int at) {
  const gchar *addr = hops[at].host[0].addr;
  if (!addr) return;
  int i;
  for (i = at; i > 0; i--) {
    if (hops[i - 1].reach) break;
    if (STR_NEQ(addr, hops[i - 1].host[0].addr)) break;
  }
  if (hops_no != (i + 1)) set_hops_no(i + 1, "unreachable duplicates");
}

static void update_stat(int at, int rtt, t_tseq *mark, int rxtx) {
  if (rxtx & RX) hops[at].recv++;
  if (rxtx & TX) hops[at].sent++;
  if (rxtx && hops[at].sent) hops[at].loss = (hops[at].sent - hops[at].recv) * 100. / hops[at].sent;
  if (rtt >= 0) {
    if ((rtt < hops[at].best) || (hops[at].best < 0)) hops[at].best = rtt;
    if ((rtt > hops[at].wrst) || (hops[at].wrst < 0)) hops[at].wrst = rtt;
    if (hops[at].avrg < 0) hops[at].avrg = 0; // initiate avrg
    if (hops[at].recv > 0) hops[at].avrg += (rtt - hops[at].avrg) / hops[at].recv;
    if ((hops[at].prev > 0) && (hops[at].recv > 1)) {
      if (hops[at].jttr < 0) hops[at].jttr = 0; // initiate jttr
      hops[at].jttr += (abs(rtt - hops[at].prev) - hops[at].jttr) / (hops[at].recv - 1);
    }
    hops[at].prev = hops[at].last;
    hops[at].last = rtt;
  }
  if (mark) hops[at].mark = *mark;
  // update timeouted flag
  if (rxtx == TX) hops[at].tout = true;
  else if (rxtx != NONE) hops[at].tout = false;
  // update global gotdata flag
  if (!pinger_state.gotdata) pinger_state.gotdata = true;
  if (!hops[at].tout) {
    if ((at > visibles) && (at < hops_no)) {
      visibles = at;
      pingtab_vis_rows(at + 1);
    }
  }
}

static int calc_rtt(int at, t_tseq *mark) {
  if (!hops[at].mark.sec) return -1;
  int rtt = (mark->sec - hops[at].mark.sec) * 1000000 + (mark->usec - hops[at].mark.usec);
  rtt = (rtt > opts.tout_usec) ? -1 : rtt; // skip timeouted responses
  return rtt;
}

static inline bool seq_accord(int prev, int curr) { return ((curr - prev) == 1); }

#define ADDRONLY(addr) ((addr) ? (addr) : UNKN)
#define ADDRNAME(addr, name) ((name) ? (name) : ADDRONLY(addr))
static inline const gchar* addrname(int ndx, t_host *host) {
  return opts.dns ? ADDRNAME(host[ndx].addr, host[ndx].name) : ADDRONLY(host[ndx].addr);
}

static const gchar *info_host(int at) {
  static gchar hostinfo_cache[MAXTTL][BUFF_SIZE];
  t_hop *hop = &(hops[at]);
  t_host *host = hop->host;
  if (host[0].addr) {
    if (hop->cached) return hostinfo_cache[at];
    gchar *s = hostinfo_cache[at];
    int l = g_snprintf(s, BUFF_SIZE, "%s", addrname(0, host));
    for (int i = 1; (i < MAX_ADDRS) && host[i].addr; i++)
      if (host[i].addr) g_snprintf(s + l, BUFF_SIZE - l, "\n%s", addrname(i, host));
    if (hop->info) snprintf(s + l, BUFF_SIZE - l, "\n%s", hop->info);
    hop->cached = true;
    LOG("hostinfo cache[%d] updated with: %s", at, s);
    return s;
  }
  return NULL;
}

enum { IWBUFF_AS_NDX, IWBUFF_CC_NDX, IWBUFF_DESC_NDX, IWBUFF_RT_NDX, IWBUFF_NDX_MAX };

static const gchar *info_whois(int at, const gchar *elem, int ndx) {
  static gchar whois_cache[MAXTTL][IWBUFF_NDX_MAX][BUFF_SIZE];
  t_whois *whois = &hops[at].whois;
  if (whois->cached) return whois_cache[at][ndx];
  gchar *s = whois_cache[at][ndx];
  g_snprintf(s, BUFF_SIZE, "%s", elem);
  whois->cached = true;
  LOG("whois cache[%d] updated with: %s", at, s);
  return s;
}

static int prec(double v) { return ((v > 0) && (v < 10)) ? (v < 0.1 ? 2 : 1) : 0; }

static const gchar* fill_stat_int(int val, gchar* buff, int size) {
  if (val >= 0) g_snprintf(buff, size, INT_FMT, val); else buff[0] = 0;
  return buff;
}

static const gchar* fill_stat_dbl(double val, gchar* buff, int size, const char *sfx, int factor) {
  if (val < 0) buff[0] = 0; else {
    if (factor) val /= factor;
    int n = prec(val);
    if (sfx) g_snprintf(buff, size, DBL_SFX, n, val, sfx);
    else g_snprintf(buff, size, DBL_FMT, n, val);
  }
  return buff;
}

static const gchar* fill_stat_rtt(int usec, gchar* buff, int size) {
  if (usec > 0) {
    double val = usec / 1000.;
    g_snprintf(buff, size, DBL_FMT, prec(val), val);
  } else buff[0] = 0;
  return buff;
}

static const gchar *stat_hop(int typ, t_hop *hop) {
  static gchar loss[NUM_BUFF_SZ];
  static gchar sent[NUM_BUFF_SZ];
  static gchar recv[NUM_BUFF_SZ];
  static gchar last[NUM_BUFF_SZ];
  static gchar best[NUM_BUFF_SZ];
  static gchar wrst[NUM_BUFF_SZ];
  static gchar avrg[NUM_BUFF_SZ];
  static gchar jttr[NUM_BUFF_SZ];
  switch (typ) {
    case ELEM_LOSS: return fill_stat_dbl(hop->loss, loss, sizeof(loss), "%", 0);
    case ELEM_SENT: return fill_stat_int(hop->sent, sent, sizeof(sent));
    case ELEM_RECV: return fill_stat_int(hop->recv, recv, sizeof(recv));
    case ELEM_LAST: return fill_stat_rtt(hop->last, last, sizeof(last));
    case ELEM_BEST: return fill_stat_rtt(hop->best, best, sizeof(best));
    case ELEM_WRST: return fill_stat_rtt(hop->wrst, wrst, sizeof(wrst));
    case ELEM_AVRG: return fill_stat_dbl(hop->avrg, avrg, sizeof(avrg), NULL, 1000);
    case ELEM_JTTR: return fill_stat_dbl(hop->jttr, jttr, sizeof(jttr), NULL, 1000);
  }
  return NULL;
}

static void set_initial_maxes(void) {
  host_max.addr = host_max.name = g_utf8_strlen(ELEM_HOST_HDR, MAXHOSTNAME);
  whois_max.as   = g_utf8_strlen(ELEM_AS_HDR,   MAXHOSTNAME);
  whois_max.cc   = g_utf8_strlen(ELEM_CC_HDR,   MAXHOSTNAME);
  whois_max.desc = g_utf8_strlen(ELEM_DESC_HDR, MAXHOSTNAME);
  whois_max.rt   = g_utf8_strlen(ELEM_RT_HDR,   MAXHOSTNAME);
}


// pub
//
void stat_init(bool clean) { // clean start or on reset
  if (clean) {
    hops_no = MAXTTL;
    visibles = 0;
    pinger_state.gotdata = false;
    pinger_state.reachable = false;
    memset(hops, 0, sizeof(hops));
  }
  for (int i = 0; i < MAXTTL; i++) {
    if (clean) hops[i].reach = true;
    hops[i].last = hops[i].best = hops[i].wrst = -1;
    hops[i].loss = hops[i].avrg = hops[i].jttr = -1;
  }
  set_initial_maxes();
}

void stat_free(void) {
  for (int at = 0; at < MAXTTL; at++) { // clear all except of mark and tout
    t_hop *hop = &hops[at];
    // clear statdata
    hop->sent = hop->recv = hop->prev = 0;
    hop->last = hop->best = hop->wrst = -1; // NA(-1) at init too
    hop->loss = hop->avrg = hop->jttr = -1;
    // clear strings
    for (int i = 0; i < MAX_ADDRS; i++) {
      UPD_STR(hop->host[i].addr, NULL);
      UPD_STR(hop->host[i].name, NULL);
    }
    UPD_STR(hop->info, NULL);
  }
  set_initial_maxes();
}

void stat_clear(bool clean) {
  stat_free();
  stat_init(clean);
  pingtab_set_error(NULL);
}

void stat_reset_cache(void) { // reset 'cached' flags
  for (int i = 0; i < MAXTTL; i++) hops[i].cached = false;
  pingtab_update_width(opts.dns ? host_max.name : host_max.addr, ELEM_HOST);
}

static void stat_up_info(int at, const gchar *info) {
  if (STR_EQ(hops[at].info, info)) return;
  UPD_STR(hops[at].info, info);
  hops[at].cached = false;
  LOG("set info[%d]: %s", at, info);
}

void stat_save_success(int at, t_ping_success *data) {
  update_addrname(at, &data->host);
  update_stat(at, data->time, &data->mark, RXTX);
  if (!hops[at].reach) hops[at].reach = true;
  // management
  int ttl = at + 1;
  if (hops_no > ttl) {
    LOG("target is reached at ttl=%d", ttl);
    set_hops_no(ttl, "behind target");
  }
  pinger_free_nth_error(at);
  if (hops[at].info) stat_up_info(at, NULL);
  if (!pinger_state.reachable) pinger_state.reachable = true;
}

void stat_save_discard(int at, t_ping_discard *data) {
  update_addrname(at, &data->host);
  seq_accord(hops[at].mark.seq, data->mark.seq)
    ? update_stat(at, calc_rtt(at, &data->mark), &data->mark, RXTX)
    : update_stat(at, -1, NULL, RX);
  if (data->reason) { // 'unreach' management
    int ttl = at + 1;
    bool reach = !g_strrstr(data->reason, "nreachable");
    if (hops[at].reach != reach) hops[at].reach = reach;
    if (reach) { if (hops_no < ttl) set_hops_no(ttl, "unreachable"); }
    else {
      if (hops_no > ttl) uniq_unreach(at);
      stat_up_info(at, data->reason);
    }
    g_free(data->reason);
  }
  pinger_free_nth_error(at);
}

void stat_save_timeout(int at, t_ping_timeout *data) {
  update_stat(at, -1, &data->mark, (hops[at].mark.seq == data->mark.seq) ? NONE : TX);
}

void stat_save_info(int at, t_ping_info *data) {
  update_addrname(at, &data->host);
//  update_stat(at, -1, &data->mark, seq_accord(hops[at].mark.seq, data->mark.seq) ? RXTX : NONE);
  stat_up_info(at, data->info);
}

void stat_last_tx(int at) { // update last 'tx' unless done before
  if (hops[at].tout) { update_stat(at, -1, NULL, TX); hops[at].tout = false; }
}

#define IW_RET(elem, ndx) { return (elem) ? info_whois(at, elem, ndx) : NULL; }

const gchar *stat_elem(int at, int typ) {
  switch (typ) {
    case ELEM_HOST: return info_host(at);
    case ELEM_AS:   IW_RET(hops[at].whois.as,   IWBUFF_AS_NDX);
    case ELEM_CC:   IW_RET(hops[at].whois.cc,   IWBUFF_CC_NDX);
    case ELEM_DESC: IW_RET(hops[at].whois.desc, IWBUFF_DESC_NDX);
    case ELEM_RT:   IW_RET(hops[at].whois.rt,   IWBUFF_RT_NDX);
    case ELEM_LOSS:
    case ELEM_SENT:
    case ELEM_RECV:
    case ELEM_LAST:
    case ELEM_BEST:
    case ELEM_WRST:
    case ELEM_AVRG:
    case ELEM_JTTR:
      return stat_hop(typ, &hops[at]);
  }
  return NULL;
}

int stat_elem_max(int typ) {
  switch (typ) {
    case ELEM_HOST: return opts.dns ? host_max.name : host_max.addr;
    case ELEM_AS:   return whois_max.as;
    case ELEM_CC:   return whois_max.cc;
    case ELEM_DESC: return whois_max.desc;
    case ELEM_RT:   return whois_max.rt;
    case ELEM_FILL: return ELEM_LEN / 2;
  }
  return ELEM_LEN;
}

