
#include <stdlib.h>

#include "common.h"
#include "stat.h"
#include "pinger.h"
#include "dns.h"
#include "whois.h"

// stat formats
#define INT_FMT "%d"
#define DBL_FMT "%.*f"

enum { ELEM_LEN = 5, NUM_BUFF_SZ = 16 };
#define NONE 0U
#define RX   1U
#define TX   2U

int tgtat = MAXTTL;
int visibles = -1;

#define IS_WHOIS_NDX(ndx) ((PE_AS <= (ndx)) && ((ndx) <= PE_RT))

static t_hop hops[MAXTTL];

static void update_addrname(int at, t_host *b) { // addr is mandatory, name not
  if (!b)
    return;
  gboolean done = false;
  int vacant = -1;
  t_hop *hop = &hops[at];
  for (uint i = 0; i < G_N_ELEMENTS(hop->host); i++) {
    t_host *a = &hop->host[i];
    if ((vacant < 0) && !a->addr)
      vacant = i;
    done = STR_EQ(a->addr, b->addr);
    if (done) {
      if (!a->name) {
        if (b->name) { // add first resolved hostname to not change it back-n-forth
          UPD_STR(a->name, b->name);
          hops[at].dns_cached = hops[at].dns_cached_nl = false;
          LOG("%s[%d]: %s=%s", HOSTNAME_HDR, at + 1, NAME_HDR, b->name);
        } else if (opts.dns)
          dns_lookup(hop, i); // otherwise run dns lookup
      }
      break;
    }
  }
  if (!done) { // add addrname
    if (vacant < 0)
      LOG("%s: %s=%d %s=%s", NOSLOTS_ERR, HOP_HDR, at, ADDR_HDR, b->addr);
    else {
      t_host *a = &hop->host[vacant];
      UPD_STR(a->addr, b->addr);
      UPD_STR(a->name, b->name);
      cleanup_whois(&hop->whois[vacant]);
      hops[at].dns_cached = hops[at].dns_cached_nl = false;
      memset(hops[at].whois_cached, false, sizeof(hops[at].whois_cached));
      memset(hops[at].whois_cached_nl, false, sizeof(hops[at].whois_cached_nl));
      LOG("%s[%d]: %s=%s %s=%s", HOSTNAME_HDR, at + 1, ADDR_HDR, b->addr, NAME_HDR, mnemo(b->name));
      if (!b->name && opts.dns)
        dns_lookup(hop, vacant);   // make dns query
      if (opts.whois)
        whois_resolv(hop, vacant); // make whois query
    }
  }
  // cleanup dups
  g_free(b->addr);
  g_free(b->name);
}

static void stop_pings_behind(int from, const char *reason) {
  for (int i = from; i < MAXTTL/*G_N_ELEMENTS(pings)*/; i++)
    pinger_nth_stop(i, reason);
}

static void set_target_at(int at, const char *info) {
  tgtat = at;
  stop_pings_behind(at, info);
  pinger_vis_rows(at);
}

static void uniq_unreach(int at) {
  const char *addr = hops[at].host[0].addr;
  if (!addr)
    return;
  int last = at;
  for (; last > 0; last--)
    if (hops[last - 1].reach || STR_NEQ(addr, hops[last - 1].host[0].addr))
      break;
  if (tgtat != (last + 1))
    set_target_at(last + 1, SKIPDUP_HDR);
}

#define STAT_AVERAGE(count, avrg, val) do { \
  (count)++;                                \
  if ((count) > 0) {                        \
    if ((avrg) < 0) (avrg) = 0;             \
    (avrg) += ((val) - (avrg)) / (count);   \
  }                                         \
} while (0)

enum { PREV, CURR };

static void update_stat(int at, int rtt, t_tseq *mark, uint rxtx) {
  if (rxtx & RX)
    hops[at].recv++;
  if (rxtx & TX)
    hops[at].sent++;
  if (rxtx && hops[at].sent)
    hops[at].loss = (hops[at].sent - hops[at].recv) * 100. / hops[at].sent;
  if (rtt > 0) {
    if ((rtt < hops[at].best) || (hops[at].best < 0))
      hops[at].best = rtt;
    if ((rtt > hops[at].wrst) || (hops[at].wrst < 0))
      hops[at].wrst = rtt;
    STAT_AVERAGE(hops[at].known_rtts, hops[at].avrg, rtt);
    if (hops[at].prev > 0)
      STAT_AVERAGE(hops[at].known_jttrs, hops[at].jttr, abs(rtt - hops[at].prev));
    hops[at].prev = hops[at].last;
    hops[at].last = rtt;
  }
  if (mark) {
    hops[at].mark = *mark;
    if (mark->seq != hops[at].rseq[CURR].seq) {
      hops[at].rseq[PREV] = hops[at].rseq[CURR];
      hops[at].rseq[CURR].rtt = rtt; hops[at].rseq[CURR].seq = mark->seq;
    }
  }
  if (!pinger_state.gotdata)
    pinger_state.gotdata = (hops[at].recv > 0);
  if (rxtx == TX)
    hops[at].tout = true;
  else if (rxtx != NONE)
    hops[at].tout = false;
  if (!hops[at].tout && (visibles < at) && (at < tgtat)) {
    visibles = at;
    pinger_vis_rows(at + 1);
  }
}

static int calc_rtt(int at, t_tseq *mark) {
  if (!hops[at].mark.sec)
    return -1;
  int rtt = (mark->sec - hops[at].mark.sec) * G_USEC_PER_SEC + (mark->usec - hops[at].mark.usec);
  return (rtt > opts.tout_usec) ? -1 : rtt; // skip timeouted responses
}

// Note: name[0] is shortcut for "" test instead of STR_NEQ(name, UNKN_FIELD)
#define ADDRNAME(addr, name) (((name) && (name)[0]) ? (name) : ADDRONLY(addr))
#define ADDRONLY(addr) ((addr) ? (addr) : UNKN_FIELD)
static inline const char* addrname(int ndx, t_host *host) {
  return opts.dns ? ADDRNAME(host[ndx].addr, host[ndx].name) : ADDRONLY(host[ndx].addr);
}

const char *stat_hop_addrname(int at) {
  static char hostinfo_cache[G_N_ELEMENTS(hops)][BUFF_SIZE];
  if ((at < 0) || ((uint)at >= G_N_ELEMENTS(hops)))
    return NULL;
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->dns_cached)
      return hostinfo_cache[at];
    char *str = hostinfo_cache[at];
    int len = snprintg(hostinfo_cache[at], BUFF_SIZE, "%s", addrname(0, host));
    if (len > 0) for (uint i = 1; (i < G_N_ELEMENTS(hop->host)) && (len < BUFF_SIZE); i++) {
      if (host[i].addr) {
        int inc = snprintg(str + len, BUFF_SIZE - len, "\n%s", addrname(i, host));
        if (inc < 0)
          break;
        len += inc;
      } else
        break;
    }
    if (hop->info && (len > 0) && (len < BUFF_SIZE))
      snprintg(str + len, BUFF_SIZE - len, "\n%s", hop->info);
    hop->dns_cached = true;
    LOG("%s: #%d %s", HOST_CUP_HDR, at + 1, mnemo(str));
    return str;
  }
  return NULL;
}

static const char *host_legend(int at) {
  static char hostinfo_nl_cache[G_N_ELEMENTS(hops)][MAXHOSTNAME];
  if ((at < 0) || ((uint)at >= G_N_ELEMENTS(hops)))
    return NULL;
  t_hop *hop = &hops[at];
  t_host *host = hop->host;
  if (host[0].addr) { // as a marker
    if (hop->dns_cached_nl)
      return hostinfo_nl_cache[at];
    char *str = hostinfo_nl_cache[at];
    snprintg(str, BUFF_SIZE, "%s", addrname(0, host));
    hop->dns_cached_nl = true;
    LOG("%s: #%d %s", HOST_CUP_HDR, at + 1, mnemo(str));
    return str;
  }
  return NULL;
}

static uint column_host(int at, t_ping_column *column, int num G_GNUC_UNUSED) { // NONNULL(2)
  t_host *host = hops[at].host;
  uint n = 0;
  for (; n < MIN(G_N_ELEMENTS(hops->host), G_N_ELEMENTS(column->cell)); n++) {
    if (!host[n].addr)
      break;
   column->cell[n] = addrname(n, host);
  }
  return n; // hosts per hop
}

static const char *info_whois(int at, int type) {
  static char whois_cache[G_N_ELEMENTS(hops)][WHOIS_NDX_MAX][BUFF_SIZE];
  if ((at < 0) || ((uint)at >= G_N_ELEMENTS(hops)))
    return NULL;
  t_hop *hop = &hops[at];
  t_whois *whois = hop->whois;
  char *view = T_WHOIS_VIEW(whois, type);
  if (!view)
    return NULL;
  char *str = whois_cache[at][type];
  // TODO: reset hops[at].whois_cached[] with changing 'whois_multi'
  if (!hop->whois_cached[type]) {
    int len = snprintg(str, BUFF_SIZE, "%s", view);
    if (len > 0) for (uint i = 1; (i < G_N_ELEMENTS(hop->whois)) && (len < BUFF_SIZE); i++) {
      view = T_WHOIS_VIEW(&whois[i], type);
      if (!view)
        break;
      int inc = snprintg(str + len, BUFF_SIZE - len, "\n%s", view);
      if (inc < 0)
        break;
      len += inc;
    }
    hop->whois_cached[type] = true;
    LOG("%s: [%d,%d] %s", WHOIS_CUP_HDR, at, type, mnemo(str));
  }
  return str;
}

static const char *whois_legend(int at, int type) {
  static char whois_nl_cache[G_N_ELEMENTS(hops)][WHOIS_NDX_MAX][MAXHOSTNAME];
  if ((at < 0) || ((uint)at >= G_N_ELEMENTS(hops)))
    return NULL;
  t_whois *whois = hops[at].whois;
  char *view = T_WHOIS_VIEW(whois, type);
  if (view) { // as a marker
    char *str = whois_nl_cache[at][type];
    if (!hops[at].whois_cached_nl[type]) {
      snprintg(str, MAXHOSTNAME, "%s", view);
      hops[at].whois_cached_nl[type] = true;
      LOG("%s: [%d,%d] %s", WHOIS_CUP_HDR, at, type, mnemo(str));
    }
    return str;
  }
  return NULL;
}

static uint column_whois(int at, t_ping_column *column, int type) { // NONNULL(2)
  t_whois *whois = hops[at].whois;
  uint n = 0;
  for (; n < MIN(G_N_ELEMENTS(hops->whois->m), G_N_ELEMENTS(column->cell)); n++) {
    const char *str = T_WHOIS_VIEW(&whois[n], type);
    if (!str)
      break;
    column->cell[n] = str;
  }
  return n; // hosts per hop
}

static int precision(double val) {
  return ((val > 0) && (val < 10)) ? ((val < 0.1) ? 2 : 1) : 0;
}

static const char* fill_stat_int(int val, char* buff, int size) {
  if (val < 0)
    buff[0] = 0;
  else
    snprintg(buff, size, INT_FMT, val);
  return buff;
}

static inline const char* fill_stat_dbl(double val, char* buff, int size, gboolean percent) {
  if (val < 0)
    buff[0] = 0;
  else
    snprintg(buff, size, percent ? DBL_FMT "%%" : DBL_FMT, precision(val), val);
  return buff;
}

static const char* fill_stat_rtt(int usec, char* buff, int size) {
  if (usec > 0) {
    double val = usec / 1000.;
    snprintg(buff, size, DBL_FMT, precision(val), val);
  } else
    buff[0] = 0;
  return buff;
}

static void stat_nth_hop_NA(t_hop *hop) { // NA(<0)
  hop->last = hop->best = hop->wrst = hop->prev = -1;
  hop->loss = hop->avrg = hop->jttr = -1;
  hop->rseq[PREV].rtt = hop->rseq[CURR].rtt = -1;
  hop->rseq[PREV].seq = hop->rseq[CURR].seq = 0;
}

static void stat_up_info(int at, const char *info) {
  if (STR_EQ(hops[at].info, info))
    return;
  UPD_STR(hops[at].info, info);
  hops[at].dns_cached = false;
  LOG("%s: #%d %s", INFO_HDR, at + 1, info);
}

//
// map functions

#define PERCENT '%'

static const char* stat_hop_whois_asn(int at) {
  return info_whois(at, WHOIS_AS_NDX);
}

static const char* stat_hop_whois_cc(int at) {
  return info_whois(at, WHOIS_CC_NDX);
}

static const char* stat_hop_whois_desc(int at) {
  return info_whois(at, WHOIS_DESC_NDX);
}

static const char* stat_hop_whois_rt(int at) {
  return info_whois(at, WHOIS_RT_NDX);
}

static const char* stat_hop_loss(int at) {
  static char buff_loss[NUM_BUFF_SZ];
  return fill_stat_dbl(hops[at].loss, buff_loss, sizeof(buff_loss), PERCENT);
}

static const char* stat_hop_sent(int at) {
  static char buff_sent[NUM_BUFF_SZ];
  return fill_stat_int(hops[at].sent, buff_sent, sizeof(buff_sent));
}

static const char* stat_hop_recv(int at) {
  static char buff_recv[NUM_BUFF_SZ];
  return fill_stat_int(hops[at].recv, buff_recv, sizeof(buff_recv));
}

static const char* stat_hop_last(int at) {
  static char buff_last[NUM_BUFF_SZ];
  return fill_stat_rtt(hops[at].last, buff_last, sizeof(buff_last));
}

static const char* stat_hop_best(int at) {
  static char buff_best[NUM_BUFF_SZ];
  return fill_stat_rtt(hops[at].best, buff_best, sizeof(buff_best));
}

static const char* stat_hop_wrst(int at) {
  static char buff_wrst[NUM_BUFF_SZ];
  return fill_stat_rtt(hops[at].wrst, buff_wrst, sizeof(buff_wrst));
}

static const char* stat_hop_avrg(int at) {
  static char buff_avrg[NUM_BUFF_SZ]; // usec -> msec
  return fill_stat_dbl(hops[at].avrg / 1000, buff_avrg, sizeof(buff_avrg), !PERCENT);
}

static const char* stat_hop_jttr(int at) {
  static char buff_jttr[NUM_BUFF_SZ]; // usec -> msec
  return fill_stat_dbl(hops[at].jttr / 1000, buff_jttr, sizeof(buff_jttr), !PERCENT);
}

//

static uint stat_ping_col_addrname(int at, t_ping_column *column) { // NONNULL(2)
  return column_host(at, column, 0);
}

static uint stat_ping_col_whois_asn(int at, t_ping_column *column) { // NONNULL(2)
  return column_whois(at, column, WHOIS_AS_NDX);
}

static uint stat_ping_col_whois_cc(int at, t_ping_column *column) { // NONNULL(2)
  return column_whois(at, column, WHOIS_CC_NDX);
}

static uint stat_ping_col_whois_desc(int at, t_ping_column *column) { // NONNULL(2)
  return column_whois(at, column, WHOIS_DESC_NDX);
}

static uint stat_ping_col_whois_rt(int at, t_ping_column *column) { // NONNULL(2)
  return column_whois(at, column, WHOIS_RT_NDX);
}


// pub
//

t_type_elem pingelem[PE_MAX] = {
  [PE_NO]   = { .type = PE_NO,   .enable = true, .name = "" },
  [PE_HOST] = { .type = PE_HOST, .enable = true, .view = stat_hop_addrname,   .multihop = stat_ping_col_addrname   },
  [PE_AS]   = { .type = PE_AS,   .enable = true, .view = stat_hop_whois_asn,  .multihop = stat_ping_col_whois_asn  },
  [PE_CC]   = { .type = PE_CC,   .enable = true, .view = stat_hop_whois_cc,   .multihop = stat_ping_col_whois_cc   },
  [PE_DESC] = { .type = PE_DESC,                 .view = stat_hop_whois_desc, .multihop = stat_ping_col_whois_desc },
  [PE_RT]   = { .type = PE_RT,                   .view = stat_hop_whois_rt,   .multihop = stat_ping_col_whois_rt   },
  [PE_FILL] = { .type = PE_FILL, .enable = true, .name = "" },
  [PE_LOSS] = { .type = PE_LOSS, .enable = true, .view = stat_hop_loss, .statview = stat_hop_loss },
  [PE_SENT] = { .type = PE_SENT, .enable = true, .view = stat_hop_sent, .statview = stat_hop_sent },
  [PE_RECV] = { .type = PE_RECV,                 .view = stat_hop_recv, .statview = stat_hop_recv },
  [PE_LAST] = { .type = PE_LAST, .enable = true, .view = stat_hop_last, .statview = stat_hop_last },
  [PE_BEST] = { .type = PE_BEST, .enable = true, .view = stat_hop_best, .statview = stat_hop_best },
  [PE_WRST] = { .type = PE_WRST, .enable = true, .view = stat_hop_wrst, .statview = stat_hop_wrst },
  [PE_AVRG] = { .type = PE_AVRG, .enable = true, .view = stat_hop_avrg, .statview = stat_hop_avrg },
  [PE_JTTR] = { .type = PE_JTTR, .enable = true, .view = stat_hop_jttr, .statview = stat_hop_jttr },
};

void stat_init(gboolean clean) { // clean start or on reset
  if (clean) {
    tgtat = MAXTTL;
    visibles = -1;
    pinger_state.gotdata = false;
    pinger_state.reachable = false;
    memset(hops, 0, sizeof(hops)); // BUFFNOLINT
  }
  t_hop *hop = hops;
  for (uint i = 0; hop && (i < G_N_ELEMENTS(hops)); i++, hop++) {
    if (clean)
      hop->reach = true;
    stat_nth_hop_NA(hop);
    hop->at = i;
  }
  stat_whois_enabler();
}

void stat_free(void) {
  for (uint at = 0; at < G_N_ELEMENTS(hops); at++) { // clear all except of mark and tout
    t_hop *hop = &hops[at];
    // clear statdata
    hop->sent = hop->recv = hop->known_rtts = hop->known_jttrs = 0;
    stat_nth_hop_NA(hop);
    // clear strings
    for (uint i = 0; i < G_N_ELEMENTS(hop->host); i++) {
      host_free(&hop->host[i]);
      cleanup_whois(&hops[at].whois[i]);
    }
    CLR_STR(hop->info);
  }
}

void stat_clear(gboolean clean) { stat_free(); stat_init(clean); }

void stat_reset_dns_cache(void) {
  for (uint i = 0; i < G_N_ELEMENTS(hops); i++)
    hops[i].dns_cached = hops[i].dns_cached_nl = false;
}

void stat_save_success(int at, t_ping_success *data) {
  update_addrname(at, &data->host);
  update_stat(at, data->time, &data->mark, RX | TX);
  if (!hops[at].reach)
    hops[at].reach = true;
  // management
  int ttl = at + 1;
  if (tgtat > ttl) {
    LOG("%s: %s=%d", REACHED_HDR, OPT_TTL_HDR, ttl);
    set_target_at(ttl, AFTER_TGT_HDR);
  }
  pinger_nth_free_error(at);
  if (hops[at].info)
    stat_up_info(at, NULL);
  if (!pinger_state.reachable)
    pinger_state.reachable = true;
}

void stat_save_discard(int at, t_ping_discard *data) {
  update_addrname(at, &data->host);
  if ((data->mark.seq - hops[at].mark.seq) != 1)
    update_stat(at, -1, NULL, RX);
  else
    update_stat(at, calc_rtt(at, &data->mark), &data->mark, RX | TX);
  // 'unreach' management
  if (data->reason) {
    int ttl = at + 1;
    gboolean reach = !g_strrstr(data->reason, "nreachable");
    if (hops[at].reach != reach)
      hops[at].reach = reach;
    if (reach) {
      if (tgtat < ttl)
        set_target_at(ttl, UNREACH_HDR);
    } else {
      if (tgtat > ttl)
        uniq_unreach(at);
      stat_up_info(at, data->reason);
    }
    g_free(data->reason);
  }
  pinger_nth_free_error(at);
}

void stat_save_timeout(int at, t_ping_timeout *data) {
  update_stat(at, -1, &data->mark, (hops[at].mark.seq == data->mark.seq) ? NONE : TX);
}

void stat_save_info(int at, t_ping_info *data) {
  update_addrname(at, &data->host);
//  update_stat(at, -1, &data->mark, seq_accord(hops[at].mark.seq, data->mark.seq) ? (RX | TX) : NONE);
  stat_up_info(at, data->info);
}

void stat_last_tx(int at) { // update last 'tx' unless done before
  if (hops[at].tout) {
    update_stat(at, -1, NULL, TX);
    hops[at].tout = false;
  }
}

double stat_dbl_elem(int at, int type) { // for json resume at exit
  double val = -1;
  switch (type) {
    case PE_LOSS: val = hops[at].loss; break;
    case PE_AVRG: val = hops[at].avrg; break;
    case PE_JTTR: val = hops[at].jttr; break;
    default: break;
  }
  return val;
}
int stat_int_elem(int at, int type) { // for json resume at exit
  int val = -1;
  switch (type) {
    case PE_SENT: val = hops[at].sent; break;
    case PE_RECV: val = hops[at].recv; break;
    case PE_LAST: val = hops[at].last; break;
    case PE_BEST: val = hops[at].best; break;
    case PE_WRST: val = hops[at].wrst; break;
    default: break;
  }
  return val;
}

#define RSEQ_FOR_SYNC(which) { data->rtt = hops[at].rseq[which].rtt; data->seq = -hops[at].rseq[which].seq; }

void stat_rseq(int at, t_rseq *data) { // assert('at' within range)
  if (data) {
    if (!data->seq || (data->seq == hops[at].rseq[CURR].seq))
      *data = hops[at].rseq[CURR];
    else if (data->seq == hops[at].rseq[PREV].seq)
      *data = hops[at].rseq[PREV];
    else { // for sync
      if (data->seq > hops[at].rseq[CURR].seq)
        RSEQ_FOR_SYNC(CURR) // rarely, but possible
      else
        RSEQ_FOR_SYNC(PREV); // unlikely with iputils-ping
    }
  }
}

inline t_legend stat_legend(int at) {
  return (t_legend){
    .name = host_legend(at),
    .as   = whois_legend(at, WHOIS_AS_NDX),
    .cc   = whois_legend(at, WHOIS_CC_NDX),
    .av   = stat_hop_avrg(at),
    .jt   = stat_hop_jttr(at),
  };
}

void stat_whois_enabler(void) {
  gboolean enable = false;
  for (uint i = 0; i < G_N_ELEMENTS(pingelem); i++)
    if (IS_WHOIS_NDX(pingelem[i].type) && pingelem[i].enable) {
      enable = true;
      break;
    }
  if (enable != opts.whois) {
    opts.whois = enable;
    LOG("%s: %s", WHOIS_HDR, enable ? ON_HDR : OFF_HDR);
  }
}

void stat_run_whois_resolv(void) {
  t_hop *hop = hops;
  for (int at = opts.range.min; hop && (at < opts.range.max); at++, hop++) {
    t_host *host = hop->host;
    for (uint i = 0; host && (i < G_N_ELEMENTS(hop->host)); i++, host++)
      if (host->addr)
        whois_resolv(hop, i);
  }
}

void stat_col_addrhost(int at, t_ping_column* column, gboolean num) { // NONNULL(2)
  t_host *host = hops[at].host;
  const char **cell = column->cell;
  for (uint n = 0; host && (n < MIN(G_N_ELEMENTS(hops->host), G_N_ELEMENTS(column->cell))); n++, host++, cell++) {
    if (!host->addr)
      break;
    *cell = num ? host->addr :
      STR_EQ(host->addr, host->name) ? NULL : host->name;
  }
}

